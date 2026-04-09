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

// Section 32 invariant: declaration presence in catalog_manager.h can be
// architecture-adjacent, but it must not be misread as a generic extension or
// plugin contract unless a stronger owner explicitly promotes that boundary.
// Section 36 invariant: metadata declarations here can be planner-adjacent,
// but they do not by themselves prove mature statistics-driven planning or a
// complete optimizer cost model.
// Section 37 invariant: durable catalog ownership and metadata-root authority
// live here, but declaration presence still does not imply global invalidation
// coherence or mature online schema change guarantees.

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
#include "scratchbird/core/config.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/tablespace.h"
#include "scratchbird/optimizer/statistics.h"

#if defined(_WIN32)
#ifdef ERROR
#undef ERROR
#endif
#ifdef IN
#undef IN
#endif
#ifdef OUT
#undef OUT
#endif
#ifdef ABSOLUTE
#undef ABSOLUTE
#endif
#ifdef RELATIVE
#undef RELATIVE
#endif
#ifdef DELETE
#undef DELETE
#endif
#ifdef PASS
#undef PASS
#endif
#ifdef FAIL
#undef FAIL
#endif
#ifdef NO_DATA
#undef NO_DATA
#endif
#endif

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
    struct AuditIntegrityResult;

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
        // Memory estimate: page_size * MAX_BATCH_SIZE_PAGES + ~32 bytes/page TID map overhead
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
            ID parent_schema_id;                // Parent schema UUID (zero UUID for top-level schemas)
            std::string schema_name;            // Short name (not full path)
            bool name_is_delimited = false;     // True if name was double-quoted (case-sensitive)
            std::string full_path;              // Cached full dotted path (e.g., "emulation.firebird")
            SchemaType schema_type = SchemaType::APPLICATION;
            ID owner_id;                        // Owner UUID reference (NOT name)
            uint16_t default_tablespace_id = 0; // Internal numeric tablespace ID (0 = primary)
            ID default_tablespace_uuid{};       // Catalog UUID for tablespace (SBDB$KEY_TABLESPACE)
            uint32_t permissions = 0;           // Bitmask of schema permissions
            uint16_t default_charset = 0;       // Default character set (0 = inherit)
            ID default_charset_uuid{};          // Catalog UUID for charset (SBDB$KEY_CHARSET)
            uint16_t reserved = 0;
            uint32_t default_collation_id = 0;  // Default collation ID (0 = inherit from database)
            ID acl_oid{};               // TOAST reference for ACL (IMPLEMENTED)
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
            uint16_t tablespace_id = 0;        // Internal numeric tablespace ID (0 = primary)
            ID tablespace_uuid{};              // Catalog UUID for tablespace (SBDB$KEY_TABLESPACE)
            uint16_t default_charset = 0;      // Default character set (0 = inherit from schema)
            ID default_charset_uuid{};         // Catalog UUID for charset (SBDB$KEY_CHARSET)
            uint32_t default_collation_id = 0; // Default collation ID (0 = inherit from schema)
            ID storage_params_oid{};   // TOAST reference for storage parameters - IMPLEMENTED
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

        struct ViewColumnBinding
        {
            std::string view_column;
            ID base_table_id{};
            std::string base_column;
            bool writable = false;
        };

        // View information (ALPHA Phase 1 - Views)
        struct ViewInfo {
            ID view_id;
            ID schema_id;
            std::string name;
            bool name_is_delimited = false;    // True if name was double-quoted (case-sensitive)
            ID owner_id;
            std::string definition;  // Original SQL text retained for reference/introspection only
            std::string source_dialect;  // Canonical dialect/profile id for the original SQL text
            std::vector<uint8_t> compiled_query_sblr;  // Canonical execution form for the stored view body
            std::vector<uint8_t> native_compiled_code;  // Optional SBLR->native artifact bytes
            bool check_option;
            bool security_definer = false;    // SECURITY DEFINER view
            bool security_barrier = false;    // SECURITY BARRIER view
            std::vector<std::string> column_names;  // Optional explicit columns
            std::vector<ViewColumnBinding> insert_bindings;  // Persisted view-column to base-column map
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
            uint16_t physical_data_type = 0; // On-disk tuple encoding type (0 = same as data_type)
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
            ID generation_expr_oid{};   // TOAST reference for large expressions
            std::vector<uint16_t> dependent_columns;  // Column ordinals this depends on

            // IDENTITY column fields (ALPHA Phase 1 - Constraint Features)
            bool is_identity = false;           // Is this an IDENTITY column?
            bool identity_always = true;        // true=ALWAYS (cannot override), false=BY DEFAULT (can override)
            ID identity_sequence_id;            // Associated sequence ID (zero if not identity)

            uint8_t storage_type = 0;       // TOAST storage strategy
            bool with_timezone = false;     // For TIMESTAMP: WITH TIME ZONE
            uint16_t charset = 0;           // Character set (0 = inherit from table)
            ID charset_uuid{};              // Catalog UUID for charset (SBDB$KEY_CHARSET)
            ID domain_id;                   // WP-2 CAT-M7: Domain ID (zero if not domain-based)
            bool is_array = false;          // Array column flag (true when column stores array values)
            uint32_t array_size = 0;        // Fixed array size (0 = unspecified/unbounded)
            uint16_t timezone_hint = 0;     // Timezone ID for display (0 = use connection default)
            ID timezone_uuid{};             // Catalog UUID for timezone (SBDB$KEY_TIMEZONE)
            uint32_t collation_id = 0;      // Collation ID (0 = inherit from table)
            std::string default_value;      // Serialized default (simple literals)
            std::string default_expr;       // DEFAULT expression (hex bytecode, ALPHA Phase A)
            ID default_value_oid{}; // TOAST reference for large defaults
            std::string check_expr;         // CHECK constraint expression (hex bytecode)
            ID check_expr_oid{};    // TOAST reference for check expressions
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
            ZONEMAP = 13,     // Zone map (min/max) index
            ART = 0x0E,              // Adaptive radix tree index
            BLOOM = 0x0F,            // Bloom filter range index
            VECTOR_FLAT = 0x10,       // Brute-force float vector index
            VECTOR_BIN_FLAT = 0x11,   // Brute-force binary vector index
            IVF_FLAT = 0x12,          // IVF flat vector variant
            BIN_IVF_FLAT = 0x13,      // IVF flat binary variant
            IVF_PQ = 0x14,            // IVF product quantization variant
            IVF_SQ8 = 0x15,           // IVF scalar quantization variant
            IVF_SQ8_HYBRID = 0x16,    // IVF SQ8 with deterministic hybrid routing
            RHNSW_PQ = 0x17,          // HNSW with PQ payload variant
            RHNSW_SQ = 0x18,          // HNSW with SQ payload variant
            ANNOY = 0x19,             // ANNOY random projection forest ANN
            NSG = 0x1A,               // NSG graph ANN index
            DISKANN = 0x1B,           // DiskANN graph ANN index
            SCANN = 0x1C,             // ScaNN partitioned ANN index
            GPU_CAGRA = 0x1D,         // GPU CAGRA graph ANN index
            MINHASH_LSH = 0x1E,       // MinHash LSH index
            SPARSE_INVERTED = 0x1F,   // Sparse inverted index
            SPARSE_WAND = 0x20,       // Sparse WAND index
            TRIE = 0x21,              // Radix trie index
            INVERTED = 0x22,          // Generic inverted index profile
            STL_SORT = 0x23,          // Sorted-list profile (B-tree runtime)
            NGRAM = 0x24,             // N-gram index
            MONGODB_2D = 0x25,                // MongoDB planar 2d geospatial index
            MONGODB_2DSPHERE = 0x26,          // MongoDB spherical 2dsphere index
            MONGODB_2DSPHERE_BUCKET = 0x27,   // MongoDB 2dsphere bucket (time-series) index
            MONGODB_GEO_HAYSTACK = 0x28,      // MongoDB geoHaystack index
            MONGODB_WILDCARD = 0x29,          // MongoDB wildcard path/value index
            MONGODB_ENCRYPTED_RANGE = 0x2A,   // MongoDB encrypted range index
            NEO4J_LOOKUP = 0x2B,              // Neo4j lookup index (label/reltype token map)
            NEO4J_TEXT = 0x2C,                // Neo4j text index (contains/startsWith/endsWith)
            NEO4J_RANGE = 0x2D,               // Neo4j range index
            NEO4J_POINT = 0x2E,               // Neo4j point index (space-filling curve key)
            NEO4J_VECTOR = 0x2F,              // Neo4j vector index
            CASSANDRA_SASI = 0x30,            // Cassandra SASI index
            CASSANDRA_SAI = 0x31,             // Cassandra SAI index
            REDIS_STRING = 0x32,              // Redis string structure index
            REDIS_HASH = 0x33,                // Redis hash structure index
            REDIS_LIST = 0x34,                // Redis list structure index
            REDIS_SET = 0x35,                 // Redis set structure index
            REDIS_ZSET = 0x36,                // Redis sorted-set structure index
            REDIS_STREAM = 0x37,              // Redis stream structure index
            REDIS_BITMAP = 0x38,              // Redis bitmap structure index
            REDIS_HLL = 0x39,                 // Redis HyperLogLog structure index
            REDIS_GEO = 0x3A                  // Redis geo structure index
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
            uint16_t tablespace_id = 0;    // Internal numeric tablespace ID
            ID tablespace_uuid{};          // Catalog UUID for tablespace (SBDB$KEY_TABLESPACE)
            IndexType index_type = IndexType::BTREE;
            std::string physical_family;   // Canonical admitted family identity
            std::string planner_family;    // Optimizer metrics/costing family substrate
            std::string family_mode;       // DIRECT, SHARED_RUNTIME, QUERY_ALIAS, etc.
            uint16_t format_version = 1;
            std::string alias_origin;      // Non-empty only when created through an alias surface
            uint16_t family_options_version = 1;
            std::string lifecycle_model;   // Canonical lifecycle identity
            optimizer::IndexFamilyMetricsType metrics_type =
                optimizer::IndexFamilyMetricsType::UNKNOWN;
            uint16_t metrics_version = 1;
            std::string queryability_state; // BUILDING, QUERYABLE, RETIRING, FAILED, etc.
            bool is_unique = false;
            std::vector<ID> column_ids;
            std::vector<ID> include_column_ids;
            ID index_params_oid{}; // TOAST reference for index parameters - IMPLEMENTED
            uint64_t created_time = 0;
            uint32_t collation_id = 100; // Default: utf8_bin (binary comparison)
                                         // Textual indexes may override this explicitly.

            // R-tree specific parameters (Phase 2 Task 9.2)
            uint32_t rtree_max_entries = 50; // Maximum entries per R-tree node (M parameter)

            // Task 17: Expression and Filtered Indexes
            bool is_expression_index = false;      // Index on expression(s) rather than columns
            bool is_partial_index = false;         // Index with WHERE clause (filtered)
            ID expression_oid{};           // TOAST reference for serialized expression tree(s)
            ID predicate_oid{};            // TOAST reference for serialized WHERE predicate
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
            ID check_expr_oid{};      // TOAST reference for large expressions

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

        enum class JobGroup : uint8_t
        {
            USER_DEFINED = 0,
            SYSTEM_LOCAL = 1,
            MANAGEMENT = 2,
            GROUP = 3,
            CLUSTER = 4,
            IT_MANAGEMENT = 5,
            OLAP = 6
        };

        enum class JobParamType : uint8_t
        {
            BOOL = 0,
            INT = 1,
            FLOAT = 2,
            STRING = 3,
            UUID = 4,
            JSON = 5
        };

        struct JobInfo
        {
            ID job_id;
            std::string job_name;
            std::string description;
            JobClass job_class = JobClass::UNSPECIFIED;
            JobType job_type = JobType::SQL;
            std::string job_sql;  // Original SQL text retained for reference/introspection only
            std::vector<uint8_t> bytecode;  // Canonical SBLR execution form for SQL jobs
            std::string source_dialect;  // Canonical dialect/profile id for the original SQL text
            std::vector<uint8_t> native_compiled_code;  // Optional SBLR->native artifact bytes
            ID procedure_uuid;
            std::string external_command;
            ScheduleKind schedule_kind = ScheduleKind::CRON;
            std::string cron_expression;
            int64_t interval_seconds = 0;
            uint64_t starts_at = 0;
            uint64_t ends_at = 0;
            std::string schedule_tz;
            bool has_measurement = false;
            std::string measurement_options;
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

        struct JobTypeCatalogInfo
        {
            ID job_type_id;
            std::string job_type_name;
            JobGroup job_group = JobGroup::USER_DEFINED;
            bool is_system = false;
            bool is_enabled = true;
            uint32_t default_timeout_ms = 0;
            uint16_t default_max_retries = 0;
            uint8_t default_priority = 0;
            bool has_description = false;
            std::string description;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct JobTypeParamCatalogInfo
        {
            ID param_id;
            ID job_type_id;
            std::string param_key;
            JobParamType param_type = JobParamType::STRING;
            bool is_required = false;
            bool has_default_value = false;
            std::string default_value;
            bool has_description = false;
            std::string description;
            bool is_valid = true;
        };

        struct JobParamCatalogInfo
        {
            ID param_id;
            ID job_id;
            std::string param_key;
            JobParamType param_type = JobParamType::STRING;
            std::string param_value;
            bool is_valid = true;
        };

        struct JobScheduleCatalogInfo
        {
            ID schedule_id;
            ScheduleKind schedule_kind = ScheduleKind::CRON;
            bool has_interval_ms = false;
            uint64_t interval_ms = 0;
            bool has_cron_expr = false;
            std::string cron_expr;
            bool is_enabled = true;
            bool is_valid = true;
        };

        struct JobTypePolicyCatalogInfo
        {
            ID policy_id;
            ID job_type_id;
            uint16_t max_concurrent = 0;
            bool is_enabled = true;
            bool is_valid = true;
        };

        enum class RemoteConnectorState : uint8_t
        {
            DISABLED = 0,
            PROBING = 1,
            READY = 2,
            DEGRADED = 3,
            FAILED = 4
        };

        enum class RemoteSnapshotKind : uint8_t
        {
            FULL = 0,
            INCREMENTAL = 1,
            CAPABILITY_REFRESH = 2
        };

        enum class RemoteSnapshotStatus : uint8_t
        {
            STARTED = 0,
            RUNNING = 1,
            COMPLETE = 2,
            FAILED = 3,
            CANCELLED = 4
        };

        enum class RemoteObjectKind : uint8_t
        {
            SCHEMA = 0,
            TABLE = 1,
            VIEW = 2,
            INDEX = 3,
            SEQUENCE = 4,
            PROCEDURE = 5,
            FUNCTION = 6,
            TRIGGER = 7,
            DOMAIN = 8,
            TYPE = 9
        };

        enum class RemoteSchemaMappingMode : uint8_t
        {
            EXACT = 0,
            PREFIX = 1,
            REGEX = 2
        };

        enum class RemoteTxnMode : uint8_t
        {
            NONE = 0,
            AUTO = 1,
            AUTONOMOUS = AUTO,
            JOINED = 2,
            JOIN_LOCAL = JOINED,
            XA_PREPARED = 3,
            READ_ONLY_SNAPSHOT = 4
        };

        enum class RemoteTxnState : uint8_t
        {
            ACTIVE = 0,
            PREPARED = 1,
            COMMITTED = 2,
            ROLLED_BACK = 3,
            ABORTED = 4
        };

        enum class RemoteExecStatus : uint8_t
        {
            SUCCESS = 0,
            FAILED = 1,
            TIMEOUT = 2,
            CANCELLED = 3
        };

        enum class RemoteErrorClass : uint8_t
        {
            CONNECTION = 0,
            AUTH = 1,
            CAPABILITY = 2,
            METADATA = 3,
            EXECUTION = 4,
            TIMEOUT = 5,
            TRANSACTION = 6,
            POLICY = 7,
            INTERNAL = 8
        };

        enum class RemoteOperationClass : uint8_t
        {
            QUERY = 0,
            DML = 1,
            DDL = 2,
            ADMIN = 3,
            PROCEDURAL = 4,
            METADATA = 5,
            TXN_CONTROL = 6
        };

        struct RemoteConnectorCatalogInfo
        {
            ID remote_connector_id;
            ID fdw_server_id;
            ID fdw_id;
            std::string connector_name;
            std::string engine_name;
            bool has_engine_version_text = false;
            std::string engine_version_text;
            std::string endpoint_uri;
            bool has_default_mapping_id = false;
            ID default_mapping_id;
            bool has_policy_id = false;
            ID policy_id;
            RemoteConnectorState state = RemoteConnectorState::DISABLED;
            uint32_t failure_count = 0;
            bool has_last_probe_time = false;
            uint64_t last_probe_time = 0;
            bool has_last_ready_time = false;
            uint64_t last_ready_time = 0;
            uint32_t module_checksum = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct RemoteConnectorCapabilityCatalogInfo
        {
            ID capability_id;
            ID remote_connector_id;
            std::string capability_key;
            std::string capability_group;
            std::string capability_value_json;
            bool has_source_version_text = false;
            std::string source_version_text;
            bool is_enabled = true;
            uint64_t discovered_time = 0;
            bool is_valid = true;
        };

        struct RemoteMetadataSnapshotCatalogInfo
        {
            ID snapshot_id;
            ID remote_connector_id;
            uint64_t snapshot_seq = 0;
            RemoteSnapshotKind snapshot_kind = RemoteSnapshotKind::FULL;
            RemoteSnapshotStatus snapshot_status = RemoteSnapshotStatus::STARTED;
            bool has_engine_version_text = false;
            std::string engine_version_text;
            uint32_t object_count = 0;
            uint32_t column_count = 0;
            bool has_catalog_hash = false;
            uint32_t catalog_hash = 0;
            uint64_t started_time = 0;
            bool has_completed_time = false;
            uint64_t completed_time = 0;
            bool has_error_id = false;
            ID error_id;
            bool is_valid = true;
        };

        struct RemoteMetadataObjectCatalogInfo
        {
            ID remote_object_id;
            ID snapshot_id;
            std::string remote_path;
            bool has_remote_schema_name = false;
            std::string remote_schema_name;
            std::string remote_object_name;
            RemoteObjectKind remote_object_kind = RemoteObjectKind::TABLE;
            uint32_t remote_signature = 0;
            bool has_definition_json = false;
            std::string definition_json;
            bool has_mapped_local_object_id = false;
            ID mapped_local_object_id;
            bool has_mapped_local_schema_id = false;
            ID mapped_local_schema_id;
            bool is_supported = true;
            bool is_valid = true;
        };

        struct RemoteMetadataColumnCatalogInfo
        {
            ID remote_column_id;
            ID remote_object_id;
            uint32_t ordinal_position = 0;
            std::string column_name;
            std::string remote_type_name;
            bool has_normalized_domain_id = false;
            ID normalized_domain_id;
            bool is_nullable = true;
            bool has_default_expr_text = false;
            std::string default_expr_text;
            bool has_precision_value = false;
            uint32_t precision_value = 0;
            bool has_scale_value = false;
            uint32_t scale_value = 0;
            bool has_length_value = false;
            uint32_t length_value = 0;
            bool has_charset_name = false;
            std::string charset_name;
            bool has_collation_name = false;
            std::string collation_name;
            bool has_extra_json = false;
            std::string extra_json;
            bool is_valid = true;
        };

        struct RemoteSchemaMappingCatalogInfo
        {
            ID schema_mapping_id;
            ID remote_connector_id;
            std::string mapping_name;
            std::string remote_schema_pattern;
            ID local_schema_id;
            RemoteSchemaMappingMode mapping_mode = RemoteSchemaMappingMode::EXACT;
            std::string include_object_kinds;
            bool has_exclude_object_patterns = false;
            std::string exclude_object_patterns;
            bool has_rename_rule_json = false;
            std::string rename_rule_json;
            bool has_last_snapshot_id = false;
            ID last_snapshot_id;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct RemotePassthroughPolicyCatalogInfo
        {
            ID remote_policy_id;
            ID remote_connector_id;
            bool allow_query = true;
            bool allow_dml = false;
            bool allow_ddl = false;
            bool allow_admin = false;
            bool allow_procedural = false;
            bool allow_join_local_txn = false;
            uint64_t max_rows = 0;
            uint64_t max_bytes = 0;
            uint32_t timeout_ms = 0;
            bool has_required_capabilities = false;
            std::string required_capabilities;
            std::string audit_level;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct RemotePreparedStatementCatalogInfo
        {
            ID remote_prepared_id;
            ID remote_connector_id;
            ID session_id;
            std::string statement_name;
            uint32_t statement_fingerprint = 0;
            std::string command_text;
            bool has_parameter_signature = false;
            uint32_t parameter_signature = 0;
            std::string remote_handle;
            uint64_t created_time = 0;
            uint64_t last_used_time = 0;
            bool has_expires_time = false;
            uint64_t expires_time = 0;
            bool is_valid = true;
        };

        struct RemoteTxnBindingCatalogInfo
        {
            ID remote_txn_binding_id;
            ID remote_connector_id;
            ID session_id;
            uint64_t txid = 0;
            RemoteTxnMode txn_mode = RemoteTxnMode::NONE;
            RemoteTxnState txn_state = RemoteTxnState::ACTIVE;
            std::string remote_txn_token;
            uint64_t begin_time = 0;
            bool has_terminal_time = false;
            uint64_t terminal_time = 0;
            bool has_last_heartbeat = false;
            uint64_t last_heartbeat = 0;
            bool has_last_error_id = false;
            ID last_error_id;
            bool is_valid = true;
        };

        struct RemoteExecutionAuditCatalogInfo
        {
            ID remote_exec_audit_id;
            ID remote_connector_id;
            ID session_id;
            bool has_txid = false;
            uint64_t txid = 0;
            ID request_id;
            RemoteOperationClass operation_class = RemoteOperationClass::QUERY;
            uint32_t statement_fingerprint = 0;
            bool used_prepared = false;
            RemoteTxnMode txn_mode = RemoteTxnMode::NONE;
            RemoteExecStatus exec_status = RemoteExecStatus::SUCCESS;
            uint64_t rows_returned = 0;
            uint64_t rows_affected = 0;
            uint64_t bytes_in = 0;
            uint64_t bytes_out = 0;
            uint32_t latency_ms = 0;
            uint64_t started_time = 0;
            uint64_t finished_time = 0;
            bool has_error_id = false;
            ID error_id;
            bool is_valid = true;
        };

        struct RemoteErrorCatalogInfo
        {
            ID remote_error_id;
            ID remote_connector_id;
            RemoteErrorClass error_class = RemoteErrorClass::INTERNAL;
            bool has_remote_code = false;
            std::string remote_code;
            std::string mapped_code;
            std::string message_text;
            uint64_t first_seen_time = 0;
            uint64_t last_seen_time = 0;
            uint32_t occurrence_count = 1;
            bool is_open = true;
            bool is_valid = true;
        };

        enum class SubscriptionTableState : uint8_t
        {
            INIT = 0,
            DATA_COPY = 1,
            CATCHUP = 2,
            READY = 3,
            ERROR = 4
        };

        struct ExtensionCatalogInfo
        {
            ID extension_id;
            std::string extension_name;
            ID schema_id;
            std::string version;
            ID owner_id;
            bool is_relocatable = false;
            bool has_config_id = false;
            ID config_id;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct PublicationCatalogInfo
        {
            ID publication_id;
            std::string publication_name;
            ID owner_id;
            bool publish_insert = true;
            bool publish_update = true;
            bool publish_delete = true;
            bool publish_truncate = false;
            bool publish_via_partition_root = false;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct PublicationTableCatalogInfo
        {
            ID publication_table_id;
            ID publication_id;
            ID table_id;
            bool has_column_list_id = false;
            ID column_list_id;
            bool has_where_expr_sblr_id = false;
            ID where_expr_sblr_id;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct PublicationSchemaCatalogInfo
        {
            ID publication_schema_id;
            ID publication_id;
            ID schema_id;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct SubscriptionCatalogInfo
        {
            ID subscription_id;
            std::string subscription_name;
            ID owner_id;
            bool has_connection_info_id = false;
            ID connection_info_id;
            bool enabled = true;
            bool has_slot_name = false;
            std::string slot_name;
            bool sync_commit = true;
            bool copy_data = true;
            bool create_slot = true;
            bool refresh_on_start = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct SubscriptionTableCatalogInfo
        {
            ID subscription_table_id;
            ID subscription_id;
            ID table_id;
            SubscriptionTableState state = SubscriptionTableState::INIT;
            bool has_last_error = false;
            std::string last_error;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        enum class ReplicationDirection : uint8_t
        {
            ONE_WAY = 0,
            BIDIRECTIONAL = 1
        };

        enum class ReplicationChannelState : uint8_t
        {
            INIT = 0,
            SNAPSHOT = 1,
            CATCHUP = 2,
            STREAMING = 3,
            PAUSED = 4,
            DEGRADED = 5,
            FENCED = 6,
            STOPPED = 7,
            FAILED = 8
        };

        enum class ReplicationMemberRole : uint8_t
        {
            PUBLISHER = 0,
            SUBSCRIBER = 1,
            PEER = 2
        };

        enum class ReplicationCursorState : uint8_t
        {
            ACTIVE = 0,
            STALLED = 1,
            ERROR = 2,
            CLOSED = 3
        };

        enum class ReplicationTxnState : uint8_t
        {
            RECEIVED = 0,
            VALIDATED = 1,
            APPLIED = 2,
            SKIPPED = 3,
            FAILED = 4,
            CONFLICT = 5
        };

        enum class ReplicationRetryState : uint8_t
        {
            QUEUED = 0,
            RUNNING = 1,
            EXHAUSTED = 2,
            DEAD_LETTER = 3
        };

        enum class ReplicationDdlPolicy : uint8_t
        {
            BLOCK = 0,
            MANUAL_APPROVE = 1,
            SAFE_ONLY = 2,
            FULL = 3
        };

        enum class ReplicationConflictPolicy : uint8_t
        {
            SOURCE_WINS = 0,
            TARGET_WINS = 1,
            LAST_COMMIT_WINS = 2,
            ORIGIN_PRIORITY = 3,
            MANUAL_REQUIRED = 4
        };

        enum class ReplicationConflictKind : uint8_t
        {
            UPDATE_UPDATE = 0,
            DELETE_UPDATE = 1,
            UNIQUE_CONSTRAINT = 2,
            DDL_DML = 3,
            DDL_DDL = 4,
            TYPE_MISMATCH = 5
        };

        enum class ReplicationResolutionState : uint8_t
        {
            OPEN = 0,
            AUTO_RESOLVED = 1,
            MANUAL_PENDING = 2,
            MANUAL_RESOLVED = 3,
            IGNORED = 4
        };

        enum class ReplicationEventKind : uint8_t
        {
            CHANNEL_START = 0,
            CHANNEL_PAUSE = 1,
            CHANNEL_RESUME = 2,
            CHANNEL_STOP = 3,
            LAG_ALERT = 4,
            SPLIT_BRAIN_DETECTED = 5,
            SPLIT_BRAIN_CLEARED = 6,
            RECOVERY_START = 7,
            RECOVERY_COMPLETE = 8
        };

        struct ReplicationChannelCatalogInfo
        {
            ID replication_channel_id;
            std::string channel_name;
            ReplicationDirection direction = ReplicationDirection::ONE_WAY;
            ReplicationChannelState channel_state = ReplicationChannelState::INIT;
            uint64_t mode_version = 1;
            bool has_publication_id = false;
            ID publication_id;
            bool has_subscription_id = false;
            ID subscription_id;
            bool has_source_server_id = false;
            ID source_server_id;
            bool has_target_server_id = false;
            ID target_server_id;
            ReplicationDdlPolicy ddl_policy = ReplicationDdlPolicy::SAFE_ONLY;
            ReplicationConflictPolicy conflict_policy = ReplicationConflictPolicy::MANUAL_REQUIRED;
            uint16_t max_retry_count = 0;
            uint64_t retry_backoff_base_ms = 0;
            uint64_t retry_backoff_max_ms = 0;
            uint64_t lag_warn_ms = 0;
            uint64_t lag_critical_ms = 0;
            uint32_t batch_max_txn = 0;
            uint64_t batch_max_bytes = 0;
            bool split_brain_fence_enabled = true;
            uint64_t split_brain_detect_window_ms = 0;
            ID created_by_id;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct ReplicationChannelMemberCatalogInfo
        {
            ID channel_member_id;
            ID replication_channel_id;
            std::string member_name;
            ReplicationMemberRole member_role = ReplicationMemberRole::SUBSCRIBER;
            bool has_fdw_server_id = false;
            ID fdw_server_id;
            bool local_endpoint = false;
            uint16_t priority_rank = 0;
            ID origin_id;
            bool is_active = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct ReplicationOriginCatalogInfo
        {
            ID origin_id;
            std::string origin_name;
            std::string origin_scope;
            uint16_t origin_priority = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct ReplicationCursorCatalogInfo
        {
            ID replication_cursor_id;
            ID replication_channel_id;
            ID channel_member_id;
            std::string cursor_name;
            ReplicationCursorState cursor_state = ReplicationCursorState::ACTIVE;
            std::string cursor_payload;
            uint64_t source_commit_seq = 0;
            bool has_source_commit_time = false;
            uint64_t source_commit_time = 0;
            uint64_t applied_commit_seq = 0;
            bool has_applied_time = false;
            uint64_t applied_time = 0;
            uint64_t lag_ms = 0;
            bool has_heartbeat_time = false;
            uint64_t heartbeat_time = 0;
            bool has_last_error_id = false;
            ID last_error_id;
            bool is_valid = true;
        };

        struct ReplicationOriginProgressCatalogInfo
        {
            ID origin_progress_id;
            ID replication_channel_id;
            ID target_member_id;
            ID origin_id;
            uint64_t max_applied_commit_seq = 0;
            bool has_max_applied_time = false;
            uint64_t max_applied_time = 0;
            bool is_valid = true;
        };

        struct ReplicationTxnBatchCatalogInfo
        {
            ID replication_batch_id;
            ID replication_channel_id;
            ID source_member_id;
            ID origin_id;
            std::string source_txn_id;
            uint64_t source_commit_seq = 0;
            uint64_t source_commit_time = 0;
            ReplicationTxnState txn_state = ReplicationTxnState::RECEIVED;
            uint32_t change_count = 0;
            uint64_t payload_bytes = 0;
            uint32_t batch_checksum = 0;
            uint64_t received_time = 0;
            bool has_applied_time = false;
            uint64_t applied_time = 0;
            uint16_t retry_count = 0;
            bool has_last_error_id = false;
            ID last_error_id;
            bool is_valid = true;
        };

        struct ReplicationApplyLogCatalogInfo
        {
            ID replication_apply_log_id;
            ID replication_batch_id;
            ID target_member_id;
            uint16_t apply_order = 0;
            ReplicationTxnState txn_state = ReplicationTxnState::RECEIVED;
            uint64_t apply_start_time = 0;
            bool has_apply_end_time = false;
            uint64_t apply_end_time = 0;
            bool has_applied_commit_seq = false;
            uint64_t applied_commit_seq = 0;
            uint64_t rows_inserted = 0;
            uint64_t rows_updated = 0;
            uint64_t rows_deleted = 0;
            uint32_t ddl_count = 0;
            bool has_error_id = false;
            ID error_id;
            bool is_valid = true;
        };

        struct ReplicationRetryQueueCatalogInfo
        {
            ID replication_retry_id;
            ID replication_batch_id;
            ReplicationRetryState retry_state = ReplicationRetryState::QUEUED;
            uint16_t retry_count = 0;
            uint64_t next_retry_time = 0;
            bool has_last_retry_time = false;
            uint64_t last_retry_time = 0;
            bool has_last_error_id = false;
            ID last_error_id;
            bool has_dead_letter_reason = false;
            std::string dead_letter_reason;
            bool is_valid = true;
        };

        struct ReplicationConflictCatalogInfo
        {
            ID replication_conflict_id;
            ID replication_channel_id;
            ID replication_batch_id;
            ReplicationConflictKind conflict_kind = ReplicationConflictKind::UPDATE_UPDATE;
            ID source_origin_id;
            bool has_target_origin_id = false;
            ID target_origin_id;
            ID target_object_id;
            bool has_target_row_id = false;
            ID target_row_id;
            uint64_t source_commit_seq = 0;
            bool has_target_commit_seq = false;
            uint64_t target_commit_seq = 0;
            std::string source_payload;
            bool has_target_payload = false;
            std::string target_payload;
            ReplicationResolutionState resolution_state = ReplicationResolutionState::OPEN;
            bool has_resolved_by_id = false;
            ID resolved_by_id;
            bool has_resolved_time = false;
            uint64_t resolved_time = 0;
            bool has_resolution_note = false;
            std::string resolution_note;
            bool is_valid = true;
        };

        struct ReplicationSplitBrainEventCatalogInfo
        {
            ID replication_split_brain_id;
            ID replication_channel_id;
            ReplicationEventKind event_kind = ReplicationEventKind::CHANNEL_START;
            uint64_t detected_time = 0;
            bool has_resolved_time = false;
            uint64_t resolved_time = 0;
            std::string detection_payload;
            bool has_resolution_payload = false;
            std::string resolution_payload;
            bool fence_applied = false;
            bool fence_cleared = false;
            bool has_approved_by_id = false;
            ID approved_by_id;
            bool is_valid = true;
        };

        struct ReplicationErrorCatalogInfo
        {
            ID replication_error_id;
            ID replication_channel_id;
            std::string source_component;
            std::string source_code;
            std::string message_text;
            bool recoverable = false;
            bool has_retry_after_ms = false;
            uint64_t retry_after_ms = 0;
            uint64_t first_seen_time = 0;
            uint64_t last_seen_time = 0;
            uint32_t occurrence_count = 1;
            bool is_open = true;
            bool is_valid = true;
        };

        enum class FabricScopeKind : uint8_t
        {
            GROUP = 0,
            CLUSTER = 1
        };

        enum class FabricLinkState : uint8_t
        {
            INIT = 0,
            CONNECTING = 1,
            READY = 2,
            DEGRADED = 3,
            FAILED = 4,
            FENCED = 5,
            DISABLED = 6
        };

        enum class FabricSessionState : uint8_t
        {
            OPENING = 0,
            ACTIVE = 1,
            CLOSING = 2,
            CLOSED = 3,
            FAILED = 4
        };

        enum class FabricTxnState : uint8_t
        {
            ACTIVE = 0,
            PREPARED = 1,
            COMMITTED = 2,
            ROLLED_BACK = 3,
            ABORTED = 4
        };

        enum class FabricTaskKind : uint8_t
        {
            ELECTION = 0,
            TRANSFER = 1,
            PASSTHROUGH_SBLR_EXECUTE = 2,
            VERIFY = 3
        };

        enum class FabricTaskState : uint8_t
        {
            QUEUED = 0,
            RUNNING = 1,
            STREAMING = 2,
            PAUSED = 3,
            SUCCESS = 4,
            FAILED = 5,
            CANCELLED = 6
        };

        enum class FabricErrorClass : uint8_t
        {
            CONNECTION = 0,
            AUTH = 1,
            TRANSACTION = 2,
            TASK = 3,
            TIMEOUT = 4,
            POLICY = 5,
            INTERNAL = 6
        };

        enum class CubeRangeKind : uint8_t
        {
            TIME = 0,
            HASH = 1,
            RANGE = 2,
            TENANT = 3
        };

        enum class OlapCompression : uint8_t
        {
            NONE = 0,
            LZ4 = 1
        };

        enum class OlapTier : uint8_t
        {
            HOT = 0,
            WARM = 1,
            COLD = 2
        };

        enum class OlapIngestState : uint8_t
        {
            QUEUED = 0,
            INGESTING = 1,
            COMMITTED = 2,
            FAILED = 3
        };

        enum class CubeStatus : uint8_t
        {
            ACTIVE = 0,
            DISABLED = 1,
            REBUILDING = 2
        };

        enum class CubeSourceKind : uint8_t
        {
            COLUMN = 0,
            EXPRESSION = 1
        };

        enum class CubeAggFunction : uint8_t
        {
            SUM = 0,
            COUNT = 1,
            MIN = 2,
            MAX = 3,
            AVG = 4,
            APPROX_COUNT_DISTINCT = 5
        };

        enum class CubeNullHandling : uint8_t
        {
            IGNORE_NULLS = 0,
            INCLUDE_NULLS = 1
        };

        enum class CubeMaterializationState : uint8_t
        {
            BUILDING = 0,
            ACTIVE = 1,
            STALE = 2,
            FAILED = 3
        };

        enum class CubeRefreshMode : uint8_t
        {
            MANUAL = 0,
            INTERVAL = 1,
            ON_COMMIT = 2,
            ON_SCHEDULE = 3
        };

        enum class CubeJobType : uint8_t
        {
            BUILD = 0,
            REFRESH = 1,
            REBUILD = 2,
            DROP = 3
        };

        enum class CubeJobState : uint8_t
        {
            QUEUED = 0,
            RUNNING = 1,
            COMPLETED = 2,
            FAILED = 3
        };

        struct ClusterFabricLinkCatalogInfo
        {
            ID cluster_fabric_link_id;
            std::string link_name;
            FabricScopeKind scope_kind = FabricScopeKind::GROUP;
            ID remote_node_id;
            bool has_remote_server_id = false;
            ID remote_server_id;
            uint8_t transport_kind = 2; // ConnectionTransport::INET
            bool has_auth_profile_id = false;
            ID auth_profile_id;
            FabricLinkState link_state = FabricLinkState::INIT;
            uint64_t mode_version = 1;
            uint16_t priority_rank = 0;
            uint32_t max_sessions = 0;
            uint32_t max_tasks = 0;
            uint64_t heartbeat_interval_ms = 0;
            uint16_t miss_threshold = 0;
            uint16_t fail_threshold = 0;
            bool has_last_heartbeat_time = false;
            uint64_t last_heartbeat_time = 0;
            bool has_last_ready_time = false;
            uint64_t last_ready_time = 0;
            ID created_by_id;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct ClusterFabricSessionCatalogInfo
        {
            ID cluster_fabric_session_id;
            ID cluster_fabric_link_id;
            ID session_id;
            ID effective_user_id;
            bool has_effective_role_id = false;
            ID effective_role_id;
            bool has_effective_group_id = false;
            ID effective_group_id;
            ID effective_schema_id;
            bool has_search_path_profile_id = false;
            ID search_path_profile_id;
            FabricSessionState session_state = FabricSessionState::OPENING;
            uint64_t opened_time = 0;
            bool has_closed_time = false;
            uint64_t closed_time = 0;
            bool has_last_activity_time = false;
            uint64_t last_activity_time = 0;
            bool is_valid = true;
        };

        struct ClusterFabricTxnCatalogInfo
        {
            ID cluster_fabric_txn_id;
            ID cluster_fabric_session_id;
            ID transaction_id;
            FabricTxnState txn_state = FabricTxnState::ACTIVE;
            uint64_t begin_time = 0;
            bool has_terminal_time = false;
            uint64_t terminal_time = 0;
            bool has_last_error_id = false;
            ID last_error_id;
            bool is_valid = true;
        };

        struct ClusterFabricTaskCatalogInfo
        {
            ID cluster_fabric_task_id;
            ID cluster_fabric_link_id;
            bool has_cluster_fabric_session_id = false;
            ID cluster_fabric_session_id;
            bool has_cluster_fabric_txn_id = false;
            ID cluster_fabric_txn_id;
            FabricTaskKind task_kind = FabricTaskKind::ELECTION;
            FabricTaskState task_state = FabricTaskState::QUEUED;
            uint8_t priority = 0;
            bool has_task_payload_id = false;
            ID task_payload_id;
            bool has_sblr_artifact_id = false;
            ID sblr_artifact_id;
            bool has_source_object_id = false;
            ID source_object_id;
            bool has_target_object_id = false;
            ID target_object_id;
            bool has_expected_fingerprint = false;
            uint32_t expected_fingerprint = 0;
            bool has_observed_fingerprint = false;
            uint32_t observed_fingerprint = 0;
            uint64_t submitted_time = 0;
            bool has_started_time = false;
            uint64_t started_time = 0;
            bool has_finished_time = false;
            uint64_t finished_time = 0;
            bool has_last_error_id = false;
            ID last_error_id;
            bool is_valid = true;
        };

        struct ClusterFabricTaskChunkCatalogInfo
        {
            ID cluster_fabric_task_chunk_id;
            ID cluster_fabric_task_id;
            uint64_t chunk_seq = 0;
            uint64_t chunk_total = 0;
            uint64_t chunk_bytes = 0;
            uint32_t chunk_checksum = 0;
            bool is_final_chunk = false;
            uint64_t sent_time = 0;
            bool has_acked_time = false;
            uint64_t acked_time = 0;
            bool is_valid = true;
        };

        struct ClusterFabricEventCatalogInfo
        {
            ID cluster_fabric_event_id;
            ID cluster_fabric_link_id;
            bool has_cluster_fabric_session_id = false;
            ID cluster_fabric_session_id;
            bool has_cluster_fabric_task_id = false;
            ID cluster_fabric_task_id;
            std::string event_kind;
            uint64_t event_time = 0;
            bool has_event_payload = false;
            std::string event_payload;
            bool has_actor_id = false;
            ID actor_id;
            bool is_valid = true;
        };

        struct ClusterFabricErrorCatalogInfo
        {
            ID cluster_fabric_error_id;
            ID cluster_fabric_link_id;
            FabricErrorClass error_class = FabricErrorClass::INTERNAL;
            std::string source_component;
            std::string source_code;
            std::string message_text;
            bool recoverable = false;
            uint64_t first_seen_time = 0;
            uint64_t last_seen_time = 0;
            uint32_t occurrence_count = 1;
            bool is_open = true;
            bool is_valid = true;
        };

        struct OlapWatermarkCatalogInfo
        {
            ID watermark_id;
            ID table_id;
            uint64_t last_ingested_txid = 0;
            uint64_t last_ingested_time = 0;
            bool is_valid = true;
        };

        struct OlapPartitionCatalogInfo
        {
            ID partition_id;
            ID table_id;
            bool has_shard_id = false;
            ID shard_id;
            CubeRangeKind range_kind = CubeRangeKind::TIME;
            bool has_range_min_bytes = false;
            std::string range_min_bytes;
            bool has_range_max_bytes = false;
            std::string range_max_bytes;
            uint64_t row_count = 0;
            uint64_t size_bytes = 0;
            OlapCompression compression = OlapCompression::NONE;
            OlapTier tier = OlapTier::HOT;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct OlapSegmentCatalogInfo
        {
            ID segment_id;
            ID partition_id;
            uint32_t segment_index = 0;
            uint64_t row_count = 0;
            uint64_t size_bytes = 0;
            bool has_min_key_bytes = false;
            std::string min_key_bytes;
            bool has_max_key_bytes = false;
            std::string max_key_bytes;
            uint64_t created_time = 0;
            bool is_valid = true;
        };

        struct OlapIngestLogCatalogInfo
        {
            ID batch_id;
            ID table_id;
            uint64_t min_txid = 0;
            uint64_t max_txid = 0;
            uint64_t row_count = 0;
            uint64_t size_bytes = 0;
            OlapIngestState ingest_state = OlapIngestState::QUEUED;
            uint64_t created_time = 0;
            bool has_completed_time = false;
            uint64_t completed_time = 0;
            bool is_valid = true;
        };

        struct CubeCatalogInfo
        {
            ID cube_id;
            ID schema_id;
            std::string cube_name;
            ID base_table_id;
            CubeStatus status = CubeStatus::ACTIVE;
            ID owner_id;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct CubeDimensionCatalogInfo
        {
            ID dimension_id;
            ID cube_id;
            std::string dimension_name;
            CubeSourceKind source_kind = CubeSourceKind::COLUMN;
            bool has_source_column_id = false;
            ID source_column_id;
            bool has_source_expr_sblr_id = false;
            ID source_expr_sblr_id;
            ID data_type_id;
            bool is_time_dimension = false;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct CubeLevelCatalogInfo
        {
            ID level_id;
            ID dimension_id;
            std::string level_name;
            ID key_expr_sblr_id;
            bool has_label_expr_sblr_id = false;
            ID label_expr_sblr_id;
            bool has_sort_expr_sblr_id = false;
            ID sort_expr_sblr_id;
            uint16_t level_order = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct CubeHierarchyCatalogInfo
        {
            ID hierarchy_id;
            ID dimension_id;
            std::string hierarchy_name;
            bool is_default = false;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct CubeHierarchyLevelCatalogInfo
        {
            ID hierarchy_level_id;
            ID hierarchy_id;
            ID level_id;
            uint16_t position = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct CubeMeasureCatalogInfo
        {
            ID measure_id;
            ID cube_id;
            std::string measure_name;
            CubeAggFunction agg_function = CubeAggFunction::SUM;
            ID source_expr_sblr_id;
            ID data_type_id;
            CubeNullHandling null_handling = CubeNullHandling::IGNORE_NULLS;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct CubeMaterializationCatalogInfo
        {
            ID materialization_id;
            ID cube_id;
            ID storage_table_id;
            uint32_t dimension_set_hash = 0;
            uint32_t measure_set_hash = 0;
            bool has_policy_group_hash = false;
            uint32_t policy_group_hash = 0;
            CubeMaterializationState state = CubeMaterializationState::BUILDING;
            uint64_t row_count = 0;
            uint64_t size_bytes = 0;
            bool has_last_refresh_time = false;
            uint64_t last_refresh_time = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct CubeRefreshPolicyCatalogInfo
        {
            ID policy_id;
            ID cube_id;
            CubeRefreshMode refresh_mode = CubeRefreshMode::MANUAL;
            bool has_interval_ms = false;
            uint64_t interval_ms = 0;
            bool has_watermark_column_id = false;
            ID watermark_column_id;
            bool has_max_staleness_ms = false;
            uint64_t max_staleness_ms = 0;
            bool is_enabled = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct CubeJobCatalogInfo
        {
            ID job_id;
            ID cube_id;
            CubeJobType job_type = CubeJobType::BUILD;
            CubeJobState state = CubeJobState::QUEUED;
            uint64_t created_time = 0;
            bool has_started_time = false;
            uint64_t started_time = 0;
            bool has_completed_time = false;
            uint64_t completed_time = 0;
            bool has_error_code = false;
            std::string error_code;
            bool has_error_message = false;
            std::string error_message;
            bool is_valid = true;
        };

        struct CubeJobStepCatalogInfo
        {
            ID step_id;
            ID job_id;
            uint16_t step_index = 0;
            std::string step_name;
            CubeJobState state = CubeJobState::QUEUED;
            bool has_started_time = false;
            uint64_t started_time = 0;
            bool has_completed_time = false;
            uint64_t completed_time = 0;
            bool is_valid = true;
        };

        struct CubeStatsCatalogInfo
        {
            ID cube_id;
            uint64_t row_count = 0;
            uint64_t size_bytes = 0;
            uint64_t last_refresh_time = 0;
            uint64_t avg_query_latency_ms = 0;
            float cache_hit_rate = 0.0f;
            bool is_valid = true;
        };

        struct TsParserCatalogInfo
        {
            ID parser_id;
            std::string parser_name;
            ID start_proc_id;
            ID gettoken_proc_id;
            ID end_proc_id;
            ID lextypes_proc_id;
            bool has_headline_proc_id = false;
            ID headline_proc_id;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct TsTemplateCatalogInfo
        {
            ID template_id;
            std::string template_name;
            ID init_proc_id;
            ID lexize_proc_id;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct TsDictionaryCatalogInfo
        {
            ID dictionary_id;
            std::string dictionary_name;
            ID template_id;
            bool has_init_options = false;
            ID init_options_uuid;
            std::string init_options_json;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct TsConfigCatalogInfo
        {
            ID config_id;
            std::string config_name;
            ID parser_id;
            bool has_default_dictionary_id = false;
            ID default_dictionary_id;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct TsConfigMapCatalogInfo
        {
            ID map_id;
            ID config_id;
            std::string token_type;
            ID dict_list_uuid;
            std::vector<ID> dictionary_ids;
            bool is_override = false;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct BlobFilterCatalogInfo
        {
            ID filter_id;
            std::string filter_name;
            int16_t input_subtype = 0;
            int16_t output_subtype = 0;
            std::string entry_point;
            std::string module_name;
            ID owner_id;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct TriggerMessageCatalogInfo
        {
            ID message_id;
            ID trigger_id;
            int16_t message_number = 0;
            std::string message_text;
            ID message_text_oid;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct ColumnDropHistoryCatalogInfo
        {
            ID history_id;
            ID table_id;
            std::string column_name;
            ID column_type_id;
            uint64_t dropped_time = 0;
            bool has_dropped_by = false;
            ID dropped_by_id;
            bool is_valid = true;
        };

        enum class SblrArtifactState : uint8_t
        {
            QUEUED = 0,
            COMPILING = 1,
            READY = 2,
            FAILED = 3,
            RETIRED = 4
        };

        enum class SblrQueueState : uint8_t
        {
            QUEUED = 0,
            RUNNING = 1,
            RETRY_WAIT = 2,
            FAILED = 3,
            COMPLETED = 4
        };

        struct SblrModuleCatalogInfo
        {
            ID module_id;
            uint64_t sblr_checksum = 0;
            std::string feature_key;
            std::string result_shape_id;
            std::string payload_schema_id;
            ID container_blob_id;
            uint32_t normalization_evidence_hash = 0;
            uint32_t statement_norm_count = 0;
            uint64_t capability_profile_version = 0;
            uint64_t created_txid = 0;
            uint64_t created_at = 0;
            bool is_valid = true;
        };

        struct SblrPlanCatalogInfo
        {
            ID plan_id;
            ID module_id;
            uint64_t catalog_epoch = 0;
            uint64_t security_epoch = 0;
            uint32_t normalization_evidence_hash = 0;
            uint64_t plan_checksum = 0;
            uint32_t dependency_count = 0;
            ID plan_blob_id;
            uint64_t created_txid = 0;
            uint64_t created_at = 0;
            bool is_valid = true;
        };

        struct SblrPlanDependencyCatalogInfo
        {
            ID plan_id;
            ID object_id;
            ObjectType object_kind = ObjectType::TABLE;
            bool is_valid = true;
        };

        struct SblrStatementNormCatalogInfo
        {
            ID module_id;
            ID statement_id;
            uint32_t statement_order = 0;
            std::string feature_key;
            std::string ast_family;
            uint16_t normalization_rule_set_id = 0;
            uint64_t clause_presence_mask_lo = 0;
            uint64_t clause_presence_mask_hi = 0;
            uint32_t clause_order_checksum = 0;
            uint32_t alias_rewrite_flags = 0;
            uint64_t created_txid = 0;
            bool is_valid = true;
        };

        struct SblrArtifactCatalogInfo
        {
            ID artifact_id;
            ID module_id;
            ID plan_id;
            ID object_uuid;
            std::string canonical_sblr_hash;
            std::string target_platform;
            std::string target_triple;
            std::string cpu_feature_profile;
            std::string native_abi_version;
            std::string compiler_id;
            std::string compiler_identity;
            std::string compiler_version;
            std::string optimization_profile;
            uint64_t security_policy_version = 0;
            SblrArtifactState artifact_state = SblrArtifactState::QUEUED;
            ID binary_blob_id;
            std::string hash_sha256;
            bool has_signature_blob_id = false;
            ID signature_blob_id;
            uint64_t catalog_epoch = 0;
            uint64_t security_epoch = 0;
            uint64_t created_txid = 0;
            uint64_t created_at = 0;
            bool has_retired_at = false;
            uint64_t retired_at = 0;
            bool is_valid = true;
        };

        struct SblrArtifactStatsCatalogInfo
        {
            ID artifact_id;
            uint64_t execution_count = 0;
            uint64_t execution_cpu_us = 0;
            uint64_t last_used_at = 0;
            uint64_t fallback_count = 0;
            uint64_t load_failure_count = 0;
            bool is_valid = true;
        };

        struct SblrCompilerTargetCatalogInfo
        {
            std::string target_name;
            std::string abi_name;
            bool enabled = true;
            std::string min_compiler_version;
            uint32_t policy_flags = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct SblrCompileQueueCatalogInfo
        {
            ID queue_id;
            ID module_id;
            std::string target_platform;
            SblrQueueState queue_state = SblrQueueState::QUEUED;
            uint8_t priority = 0;
            uint32_t attempt_count = 0;
            bool has_last_error_code = false;
            std::string last_error_code;
            uint64_t created_time = 0;
            bool has_last_attempt_time = false;
            uint64_t last_attempt_time = 0;
            bool is_valid = true;
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

        // Bootstrap authentication lifecycle state (P4-S1/W1 AUTH-001-002)
        enum class BootstrapState : uint8_t
        {
            UNINITIALIZED = 0,
            INITIALIZED = 1,
            LOCKED = 2
        };

        // AuthKey usage mode (Plan 03)
        enum class AuthKeyUsage : uint8_t
        {
            UNLIMITED = 0,
            LIMITED = 1,
            SINGLE_USE = 2
        };

        // AuthKey token scope (P4-S2/W1 AUTH-004-001)
        enum class AuthKeyScope : uint8_t
        {
            LOGIN_SESSION = 0,
            API_TOKEN = 1,
            REATTACH = 2,
            SERVICE_ACCOUNT = 3
        };

        // Optional AuthKey binding mode (P4-S2/W1 AUTH-004-001)
        enum class AuthKeyBindingKind : uint8_t
        {
            NONE = 0,
            PEER_UID = 1,
            CLIENT_NONCE = 2
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
            AuthKeyScope scope = AuthKeyScope::LOGIN_SESSION;
            AuthKeyBindingKind binding_kind = AuthKeyBindingKind::NONE;
            std::string binding_value;
            std::vector<ID> role_scope;
            std::vector<ID> group_scope;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            uint64_t last_used_time = 0;
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
            ID home_schema_id;               // Resolved home schema UUID
            ID current_schema_id;            // Current schema context
            ID search_path_profile_id;       // Persisted search-path profile UUID
            std::vector<ID> search_path_schema_ids; // Ordered search-path schema UUIDs
            std::vector<std::string> search_path;   // Ordered search-path schema paths
            ID authkey_id;                   // Bound AuthKey UUID (Plan 03)
            std::string emulation_mode;      // Dialect tag or emulation mode (Plan 03)
            uint64_t cluster_config_epoch = 0;
            uint64_t schema_epoch = 0;
            uint64_t security_epoch = 0;
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
            uint32_t lock_owner_proc_id = 0; // Detached backend slot retaining prepared locks
            uint32_t lock_count = 0; // Persisted non-version locks bound to the prepared owner
            uint64_t prepared_time = 0; // Epoch micros
            bool is_valid = true;
        };

        struct PreparedTransactionLockInfo
        {
            ID prepared_id;
            UuidV7Bytes object_uuid{};
            uint64_t page_num = 0;
            uint64_t request_time = 0;
            uint16_t offset_num = 0;
            uint8_t target_type = 0;
            uint8_t mode = 0;
            bool granted = true;
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
            ID message_oid{};        // TOAST OID for message text
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

        // ============================================================================
        // Canonical Type Catalog Structures (CAT-010)
        // ============================================================================

        enum class TypeKind : uint8_t
        {
            SCALAR = 0,
            DOMAIN = 1,
            COMPOSITE = 2,
            ENUM = 3,
            RANGE = 4,
            ARRAY = 5,
            MAP = 6,
            LIST = 7,
            VECTOR = 8,
            GEOMETRY = 9,
            JSON = 10,
            BSON = 11,
            XML = 12,
            VARIANT = 13
        };

        enum class TypeCastKind : uint8_t
        {
            IMPLICIT = 0,
            ASSIGNMENT = 1,
            EXPLICIT = 2,
            BINARY_COMPATIBLE = 3
        };

        enum class TypeModifierValueKind : uint8_t
        {
            U64 = 1,
            I64 = 2,
            F64 = 3,
            BOOL = 4,
            TEXT = 5,
            UUID = 6,
            JSON = 7
        };

        // Mirrors 15_Complex_Types/DOMAIN_EMULATION_PARAMETERS.md parameter keys.
        enum class TypeModifierKey : uint16_t
        {
            LENGTH_CHARS = 1,
            LENGTH_BYTES = 2,
            PRECISION = 3,
            SCALE = 4,
            MONEY_SCALE = 5,
            NUMERIC_MODE = 6,
            CHARSET_UUID = 7,
            COLLATION_UUID = 8,
            TIMEZONE_MODE = 9,
            TIMEZONE_DEFAULT = 10,
            INTERVAL_FIELDS = 11,
            BIT_LENGTH = 12,
            BIT_IS_VARYING = 13,
            INET_FAMILY = 14,
            CIDR_REQUIRED = 15,
            ENUM_LABELS = 16,
            ENUM_KIND = 17,
            SET_LABELS = 18,
            ENUM_COLLATION_UUID = 19,
            ELEMENT_TYPE_UUID = 20,
            KEY_TYPE_UUID = 21,
            VALUE_TYPE_UUID = 22,
            FROZEN = 23,
            VECTOR_ELEMENT_TYPE = 24,
            VECTOR_DIM = 25,
            VECTOR_METRIC = 26,
            VECTOR_SPARSE = 27,
            VECTOR_NORMALIZED = 28,
            GEOMETRY_KIND = 29,
            GEOMETRY_SRID = 30,
            GEOMETRY_DIMS = 31,
            JSON_VALIDATION = 32,
            BSON_VALIDATION = 33,
            CASSANDRA_TIME_PRECISION = 34,
            CASSANDRA_COUNTER = 35,
            CASSANDRA_TIMEUUID = 36,
            MONGO_TYPE_TAG = 37
        };

        struct TypeCatalogInfo
        {
            ID type_id;
            ID schema_id;
            std::string type_name;
            TypeKind type_kind = TypeKind::SCALAR;
            ID base_type_id;
            ID element_type_id;
            ID key_type_id;
            ID value_type_id;
            ID range_subtype_id;
            bool is_system = false;
            uint8_t system_origin = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct TypeModifierInfo
        {
            ID type_id;
            uint16_t modifier_key = 0;
            TypeModifierValueKind value_kind = TypeModifierValueKind::U64;
            std::optional<uint64_t> val_u64;
            std::optional<int64_t> val_i64;
            std::optional<double> val_f64;
            std::optional<bool> val_bool;
            std::optional<std::string> val_text;
            ID val_uuid;
            std::optional<std::string> val_json;
            bool is_valid = true;
        };

        struct TypeIoInfo
        {
            ID type_id;
            ID input_fn_id;
            ID output_fn_id;
            ID binary_input_fn_id;
            ID binary_output_fn_id;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct TypeCastInfo
        {
            ID source_type_id;
            ID target_type_id;
            TypeCastKind cast_kind = TypeCastKind::EXPLICIT;
            bool is_lossy = false;
            ID cast_fn_id;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct TypeTransformInfo
        {
            ID transform_id;
            ID type_id;
            ID language_id;
            ID from_sql_proc_id;
            ID to_sql_proc_id;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct EncodingConversionInfo
        {
            ID conversion_id;
            std::string conversion_name;
            ID source_charset_id;
            ID target_charset_id;
            ID conversion_proc_id;
            bool is_default = false;
            bool is_system = false;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        enum class DomainParamType : uint8_t
        {
            U32 = 1,
            I32 = 2,
            U64 = 3,
            I64 = 4,
            U8 = 5,
            BOOL = 6,
            STRING = 7,
            UUID = 8,
            ENUM_VALUE = 9,
            F32 = 10,
            F64 = 11
        };

        enum class DomainConstraintKind : uint8_t
        {
            NOT_NULL = 1,
            DEFAULT = 2,
            CHECK = 3
        };

        enum class DomainSecurityKind : uint8_t
        {
            MASK_FUNCTION = 1,
            AUDIT_ACCESS = 2,
            REQUIRE_PERMISSION = 3,
            ENCRYPTION = 4
        };

        enum class DomainValidationKind : uint8_t
        {
            VALIDATE_FUNCTION = 1,
            ON_VIOLATION = 2,
            ERROR_MESSAGE = 3
        };

        enum class DomainIntegrityKind : uint8_t
        {
            UNIQUE_ACROSS_DATABASE = 1,
            CASE_INSENSITIVE = 2,
            NORMALIZE_FUNCTION = 3
        };

        struct DomainParamKeyCatalogInfo
        {
            uint16_t param_key_id = 0;
            std::string param_name;
            DomainParamType param_type = DomainParamType::STRING;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct DomainParameterCatalogInfo
        {
            ID domain_id;
            uint16_t param_key_id = 0;
            DomainParamType param_type = DomainParamType::STRING;
            std::optional<uint32_t> val_u32;
            std::optional<int32_t> val_i32;
            std::optional<uint64_t> val_u64;
            std::optional<int64_t> val_i64;
            std::optional<uint8_t> val_u8;
            std::optional<bool> val_bool;
            std::optional<std::string> val_string;
            ID val_uuid;
            std::optional<int32_t> val_enum;
            std::optional<float> val_f32;
            std::optional<double> val_f64;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct DomainConstraintCatalogInfo
        {
            ID constraint_id;
            ID domain_id;
            DomainConstraintKind constraint_kind = DomainConstraintKind::CHECK;
            std::string constraint_expr_sblr;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct DomainSecurityCatalogInfo
        {
            ID security_id;
            ID domain_id;
            DomainSecurityKind security_kind = DomainSecurityKind::MASK_FUNCTION;
            std::string security_expr_sblr;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct DomainValidationCatalogInfo
        {
            ID validation_id;
            ID domain_id;
            DomainValidationKind validation_kind = DomainValidationKind::VALIDATE_FUNCTION;
            std::string validation_expr_sblr;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct DomainIntegrityCatalogInfo
        {
            ID integrity_id;
            ID domain_id;
            DomainIntegrityKind integrity_kind = DomainIntegrityKind::UNIQUE_ACROSS_DATABASE;
            std::string integrity_expr_sblr;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        enum class CollationTailoringKind : uint8_t
        {
            UCA = 1,
            LOCALE = 2,
            VENDOR_MYSQL = 3,
            VENDOR_FIREBIRD = 4,
            VENDOR_POSTGRESQL = 5,
            CUSTOM = 6
        };

        enum class ResourceBundleKind : uint8_t
        {
            UNSPECIFIED = 0,
            I18N = 1,
            TIMEZONE = 2,
            CHARSET = 3,
            COLLATION = 4,
            COMPOSITE = 5
        };

        enum class ResourceArtifactKind : uint8_t
        {
            UNSPECIFIED = 0,
            CHARSET_JSON = 1,
            CHARSET_MAP = 2,
            COLLATION_JSON = 3,
            UCA_WEIGHTS = 4,
            LOCALE_MANIFEST = 5,
            TZ_SOURCE = 6,
            TZ_TAR = 7,
            OTHER = 8
        };

        struct CharsetAliasCatalogInfo
        {
            ID alias_id;
            ID charset_id;
            ID bundle_id;
            std::string alias_name;
            std::string normalized_name;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct CollationTailoringCatalogInfo
        {
            ID tailoring_id;
            uint32_t collation_id = 0;
            ID bundle_id;
            CollationTailoringKind tailoring_kind = CollationTailoringKind::CUSTOM;
            std::optional<std::string> tailoring_json;
            std::optional<std::string> tailoring_blob;
            std::string tailoring_hash;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ResourceBundleCatalogInfo
        {
            ID bundle_id;
            ResourceBundleKind bundle_kind = ResourceBundleKind::COMPOSITE;
            std::string bundle_name;
            std::string bundle_version;
            std::string source_uri;
            std::string manifest_json;
            std::string content_hash;
            bool is_active = false;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t activated_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ResourceArtifactCatalogInfo
        {
            ID artifact_id;
            ID bundle_id;
            std::string artifact_path;
            ResourceArtifactKind artifact_kind = ResourceArtifactKind::OTHER;
            std::string content_blob;
            std::string content_hash;
            uint64_t content_size_bytes = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct TimezoneTransitionCatalogInfo
        {
            ID transition_id;
            ID timezone_id;
            ID bundle_id;
            int64_t effective_utc_epoch = 0;
            int32_t utc_offset_seconds = 0;
            bool is_dst = false;
            std::string abbreviation;
            uint32_t sequence_no = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct TimezoneLeapSecondCatalogInfo
        {
            ID leap_id;
            ID bundle_id;
            int64_t effective_utc_epoch = 0;
            int32_t total_correction_seconds = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // ============================================================================
        // Canonical reserved-word/parser capability catalog structures (CAT-014)
        // ============================================================================

        enum class EmulationEngine : uint8_t
        {
            NATIVE = 0,
            FIREBIRD = 1,
            FIREBIRDSQL = FIREBIRD,  // Canonical alias used by vNext docs
            POSTGRESQL = 2,
            MYSQL = 3,
            CASSANDRA = 4,
            MILVUS = 5,
            MONGODB = 6,
            NEO4J = 7,
            REDIS = 8,
            MARIADB = 9,
            INFLUXDB = 10,
            CLICKHOUSE = 11,
            OPENSEARCH = 12,
            DUCKDB = 13,
            UNSPECIFIED = 255
        };

        enum class StorageProfile : uint8_t
        {
            RELATIONAL = 0,
            NATIVE_EMULATION = 1,
            HYBRID = 2,
            UNSPECIFIED = 255
        };

        enum class ParserCapabilityAction : uint8_t
        {
            IMPLEMENT = 1,
            REMAP = 2,
            REJECT = 3
        };

        enum class ParserTransformStage : uint8_t
        {
            PRE_PARSE = 1,
            AST_REWRITE = 2,
            SBLR_REWRITE = 3,
            RESULT_REWRITE = 4
        };

        enum class ParserErrorSeverity : uint8_t
        {
            ERROR = 1,
            WARNING = 2,
            NOTICE = 3
        };

        enum class ParserPrecedenceTiebreak : uint8_t
        {
            SPECIFICITY_FIRST = 1,
            PROFILE_ORDER = 2,
            FEATURE_KEY_ASC = 3
        };

        struct ReservedWordCatalogInfo
        {
            ID reserved_word_id;
            std::string word;
            EmulationEngine parser_scope = EmulationEngine::NATIVE;
            bool is_reserved = true;
            bool is_keyword = true;
            uint64_t last_updated_txid = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct EmulationProfileCatalogInfo
        {
            ID emulation_profile_id;
            EmulationEngine engine = EmulationEngine::NATIVE;
            bool enabled = false;
            StorageProfile storage_profile = StorageProfile::RELATIONAL;
            std::string requested_engine_version;
            uint64_t installed_txid = 0;
            uint64_t last_modified_txid = 0;
            uint64_t config_flags = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ParserProfileCatalogInfo
        {
            ID parser_profile_id;
            std::string profile_name;
            EmulationEngine parser_engine = EmulationEngine::NATIVE;
            uint16_t version_major = 0;
            uint16_t version_minor = 0;
            bool is_native = false;
            bool is_default = false;
            bool is_enabled = true;
            ID base_profile_id;
            std::string profile_hash;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ParserTransformCatalogInfo
        {
            ID parser_transform_id;
            ID parser_profile_id;
            std::string transform_name;
            std::string feature_family;
            std::string feature_key;
            ParserTransformStage transform_stage = ParserTransformStage::AST_REWRITE;
            std::string input_contract_json;
            std::string output_contract_json;
            std::string implementation_ref;
            bool is_deterministic = true;
            bool is_idempotent = true;
            uint32_t timeout_ms = 1000;
            bool is_enabled = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ParserCapabilityCatalogInfo
        {
            ID parser_capability_id;
            ID parser_profile_id;
            std::string feature_family;
            std::string feature_key;
            ParserCapabilityAction capability_action = ParserCapabilityAction::IMPLEMENT;
            ID parser_transform_id;
            std::string reject_code;
            uint16_t precedence_rank = 0;
            std::string notes;
            bool is_enabled = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ParserErrorMapCatalogInfo
        {
            ID parser_error_map_id;
            ID parser_profile_id;
            std::string reject_code;
            std::string dialect_sqlstate;
            std::string dialect_error_code;
            ParserErrorSeverity error_severity = ParserErrorSeverity::ERROR;
            std::string message_template;
            std::optional<std::string> hint_template;
            bool is_retryable = false;
            bool is_enabled = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ParserFeaturePrecedenceCatalogInfo
        {
            ID parser_feature_precedence_id;
            ID parser_profile_id;
            std::string feature_family;
            std::string feature_key;
            uint16_t precedence_rank = 0;
            ParserPrecedenceTiebreak precedence_tiebreak =
                ParserPrecedenceTiebreak::SPECIFICITY_FIRST;
            bool is_terminal = false;
            bool is_enabled = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // ============================================================================
        // Canonical relation extension catalog structures (CAT-015)
        // ============================================================================

        enum class PartitionStrategy : uint8_t
        {
            RANGE = 0,
            LIST = 1,
            HASH = 2
        };

        enum class PartitionBoundKind : uint8_t
        {
            RANGE = 0,
            LIST = 1,
            HASH = 2,
            DEFAULT = 3
        };

        enum class InheritanceKind : uint8_t
        {
            INHERITS = 0,
            PARTITION = 1
        };

        enum class PackageMemberKind : uint8_t
        {
            PROCEDURE = 0,
            FUNCTION = 1
        };

        enum class EventStatus : uint8_t
        {
            ENABLED = 0,
            DISABLED = 1,
            SLAVESIDE_DISABLED = 2
        };

        enum class EventOnCompletion : uint8_t
        {
            DROP = 0,
            PRESERVE = 1
        };

        enum class LanguageKind : uint8_t
        {
            INTERNAL = 0,
            SQL = 1,
            PSQL = 2,
            PLPGSQL = 3,
            PLPYTHON = 4,
            PLLUA = 5,
            PLJAVASCRIPT = 6,
            PLDOTNET = 7,
            PLJAVA = 8,
            PLWASM = 9,
            CUSTOM = 10
        };

        struct PartitionedTableCatalogInfo
        {
            ID partitioned_table_id;
            ID table_id;
            PartitionStrategy strategy = PartitionStrategy::RANGE;
            ID key_columns_id;
            ID key_expr_sblr_id;
            uint32_t partition_count = 0;
            ID default_partition_id;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct PartitionCatalogInfo
        {
            ID partition_id;
            ID parent_table_id;
            ID partition_table_id;
            std::string partition_name;
            PartitionBoundKind bound_kind = PartitionBoundKind::RANGE;
            std::optional<std::string> range_min_bytes;
            std::optional<std::string> range_max_bytes;
            ID list_values_id;
            uint32_t hash_modulus = 0;
            uint32_t hash_remainder = 0;
            ID bound_expr_sblr_id;
            bool is_default = false;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct TableInheritanceCatalogInfo
        {
            ID inheritance_id;
            ID parent_table_id;
            ID child_table_id;
            InheritanceKind inheritance_kind = InheritanceKind::INHERITS;
            bool is_valid = true;
            uint64_t created_time = 0;
        };

        struct LanguageCatalogInfo
        {
            ID language_id;
            std::string language_name;
            LanguageKind language_kind = LanguageKind::SQL;
            ID handler_udr_id;
            ID inline_handler_udr_id;
            ID validator_udr_id;
            ID owner_id;
            bool is_trusted = false;
            bool is_system = false;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct EventCatalogInfo
        {
            ID event_id;
            ID schema_id;
            std::string event_name;
            ID definer_id;
            EventStatus status = EventStatus::ENABLED;
            EventOnCompletion on_completion = EventOnCompletion::PRESERVE;
            ScheduleKind schedule_kind = ScheduleKind::CRON;
            std::optional<std::string> cron_expr;
            std::optional<uint64_t> interval_ms;
            std::optional<uint64_t> starts_time;
            std::optional<uint64_t> ends_time;
            std::optional<uint64_t> last_executed_time;
            ID body_sblr_id;
            ID body_sql_id;
            std::optional<std::string> comment;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct PackageMemberCatalogInfo
        {
            ID member_id;
            ID package_id;
            std::string member_name;
            PackageMemberKind member_kind = PackageMemberKind::PROCEDURE;
            ID procedure_id;
            uint16_t position = 0;
            bool is_public = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // ============================================================================
        // Canonical index metadata extension catalog structures (CAT-016)
        // ============================================================================

        enum class IndexSortOrder : uint8_t
        {
            ASC = 0,
            DESC = 1
        };

        enum class IndexNullOrder : uint8_t
        {
            FIRST = 0,
            LAST = 1
        };

        enum class IndexOptionValueType : uint8_t
        {
            INT = 0,
            FLOAT = 1,
            BOOL = 2,
            STRING = 3,
            JSON = 4
        };

        enum class IndexOpclassFunctionKind : uint8_t
        {
            COMPARE = 0,
            CONSISTENT = 1,
            UNION = 2,
            PENALTY = 3,
            PICKSPLIT = 4,
            COMPRESS = 5,
            DECOMPRESS = 6,
            SAME = 7,
            EXTRACT_VALUE = 8,
            EXTRACT_QUERY = 9,
            TERM_NORMALIZE = 10,
            TERM_TOKENIZE = 11,
            DISTANCE = 12
        };

        enum class IndexMaintenanceKind : uint8_t
        {
            REBUILD = 0,
            REBALANCE = 1,
            COMPACT = 2,
            RELOCATE = 3
        };

        enum class IndexMaintenanceMode : uint8_t
        {
            OFFLINE = 0,
            ONLINE = 1
        };

        enum class IndexMaintenanceState : uint8_t
        {
            BUILDING_SHADOW = 0,
            APPLYING_DELTAS = 1,
            SWAPPING = 2,
            COMPLETE = 3,
            FAILED = 4
        };

        enum class IndexDeltaOp : uint8_t
        {
            INSERT = 0,
            DELETE = 1,
            UPDATE = 2
        };

        enum class IndexPageDeltaOp : uint8_t
        {
            INSERT = 0,
            DELETE = 1,
            UPDATE_SAME_KEY = 2,
            UPDATE_KEY_CHANGE = 3
        };

        enum class IndexPageDeltaMergeState : uint8_t
        {
            PENDING = 0,
            MERGING = 1,
            MERGED = 2,
            FAILED_FENCE = 3
        };

        enum class IndexHealthStatus : uint8_t
        {
            HEALTHY = 0,
            WARNING = 1,
            ERROR = 2,
            CORRUPT = 3
        };

        enum class BackupHistoryKind : uint8_t
        {
            FULL = 0,
            INCREMENTAL = 1,
            DIFFERENTIAL = 2,
            SNAPSHOT = 3,
            LOGICAL = 4
        };

        enum class BackupHistoryStatus : uint8_t
        {
            STARTED = 0,
            RUNNING = 1,
            SUCCESS = 2,
            FAILED = 3,
            CANCELLED = 4
        };

        enum class ConnectionTransport : uint8_t
        {
            LOCAL = 0,
            IPC = 1,
            INET = 2
        };

        enum class ConnectionAuthMethod : uint8_t
        {
            TRUST = 0,
            REJECT = 1,
            PASSWORD = 2,
            MD5 = 3,
            SCRAM_SHA_256 = 4,
            SCRAM_SHA_512 = 5,
            CERTIFICATE = 6,
            LDAP = 7,
            KERBEROS = 8,
            PEER = 9,
            IDENT = 10,
            RADIUS = 11,
            PAM = 12,
            TOKEN = 13
        };

        enum class RuntimeTransactionState : uint8_t
        {
            IN_PROGRESS = 0,
            COMMITTED = 1,
            ABORTED = 2,
            PREPARED = 3
        };

        enum class PrincipalKind : uint8_t
        {
            USER = 0,
            SERVICE = 1
        };

        enum class SourceScopeKind : uint8_t
        {
            ANY = 0,
            HOST_EXACT = 1,
            HOST_WILDCARD = 2,
            CIDR = 3,
            UNIX_SOCKET = 4,
            NODE_ID = 5,
            PEER_UID = 6,
            PEER_GID = 7
        };

        enum class CredentialKind : uint8_t
        {
            PASSWORD_ARGON2ID = 0,
            PASSWORD_SCRAM_SHA256 = 1,
            X509_SUBJECT = 2,
            KERBEROS_PRINCIPAL = 3,
            OIDC_SUBJECT = 4,
            SAML_SUBJECT = 5,
            API_TOKEN_HASH = 6
        };

        enum class AuthProviderKind : uint8_t
        {
            INTERNAL_ARGON2ID = 0,
            INTERNAL_SCRAM_SHA256 = 1,
            X509_MTLS = 2,
            LDAP_SIMPLE_BIND = 3,
            KERBEROS_GSSAPI = 4,
            OIDC_JWT = 5,
            SAML_ASSERTION = 6,
            PAM_CONVERSATION = 7,
            API_TOKEN = 8
        };

        enum class AuthProviderState : uint8_t
        {
            ENABLED = 0,
            DISABLED = 1,
            DEGRADED = 2
        };

        enum class AuthProviderFailMode : uint8_t
        {
            HARD_FAIL = 0,
            TRY_NEXT = 1
        };

        enum class AuthAttemptOutcome : uint8_t
        {
            SUCCESS = 0,
            FAIL = 1,
            TIMEOUT = 2,
            UNAVAILABLE = 3,
            POLICY_DENY = 4
        };

        enum class AuthPeerMode : uint8_t
        {
            DISABLED = 0,
            REQUIRED = 1,
            REQUIRED_PLUS_SCRAM = 2
        };

        enum class MfaFactorType : uint8_t
        {
            TOTP = 0,
            BACKUP_CODE = 1,
            BREAK_GLASS = 2
        };

        enum class ConnectionRuleTransportKind : uint8_t
        {
            TCP = 0,
            TLS = 1,
            MTLS = 2,
            UNIX_SOCKET = 3,
            IPC_LOCAL = 4
        };

        enum class ConnectionRuleTlsMode : uint8_t
        {
            NONE = 0,
            TLS = 1,
            MTLS = 2
        };

        enum class ConnectionRuleAction : uint8_t
        {
            ALLOW = 0,
            DENY = 1
        };

        enum class AuthAdapterOutcome : uint8_t
        {
            ACCEPT = 0,
            REJECT = 1,
            UNAVAILABLE = 2,
            TIMEOUT = 3
        };

        enum class AuthorizationSubjectType : uint8_t
        {
            USER = 0,
            ROLE = 1,
            GROUP = 2,
            TOKEN_SUBJECT = 3
        };

        enum class PolicyEffect : uint8_t
        {
            ALLOW = 0,
            DENY = 1
        };

        enum class AclCommandArityClass : uint8_t
        {
            FIXED = 0,
            VARIADIC = 1
        };

        enum class DocumentEngineTag : uint8_t
        {
            OPENSEARCH = 0,
            MONGODB = 1,
            GENERIC_DOC = 2
        };

        enum class TokenKind : uint8_t
        {
            BEARER = 0,
            API_KEY = 1,
            SERVICE_TOKEN = 2
        };

        enum class TokenScopeModel : uint8_t
        {
            GENERIC = 0,
            INFLUX = 1,
            MILVUS = 2,
            OPENSEARCH = 3,
            CLICKHOUSE = 4
        };

        enum class TokenResourceKind : uint8_t
        {
            CLUSTER = 0,
            DATABASE = 1,
            SCHEMA = 2,
            TABLE = 3,
            INDEX = 4,
            COLLECTION = 5,
            BUCKET = 6,
            MEASUREMENT = 7,
            CHANNEL = 8,
            GRAPH = 9,
            VECTOR_SPACE = 10,
            TENANT = 11
        };

        enum class BindingSubjectType : uint8_t
        {
            USER = 0,
            ROLE = 1,
            GROUP = 2,
            TENANT = 3,
            GLOBAL = 4
        };

        enum class BindingResourceScopeKind : uint8_t
        {
            GLOBAL = 0,
            DATABASE = 1,
            SCHEMA = 2,
            RESOURCE_PATTERN = 3
        };

        enum class CertKind : uint8_t
        {
            SERVER = 0,
            CLIENT = 1,
            CLUSTER = 2,
            SIGNING = 3
        };

        enum class CertStatus : uint8_t
        {
            ACTIVE = 0,
            REVOKED = 1,
            EXPIRED = 2,
            SUSPENDED = 3
        };

        enum class KeyMaterialKind : uint8_t
        {
            SYMMETRIC = 0,
            ASYMMETRIC_PRIVATE = 1,
            ASYMMETRIC_PUBLIC = 2
        };

        enum class TrustAnchorState : uint8_t
        {
            ACTIVE = 0,
            ROLLING_OUT = 1,
            RETIRED = 2
        };

        enum class TlsVersion : uint8_t
        {
            TLS_1_2 = 0,
            TLS_1_3 = 1
        };

        enum class RevocationSource : uint8_t
        {
            LOCAL = 0,
            CRL = 1,
            OCSP = 2,
            OPERATOR = 3
        };

        enum class RevocationReason : uint8_t
        {
            UNSPECIFIED = 0,
            KEY_COMPROMISE = 1,
            CA_COMPROMISE = 2,
            AFFILIATION_CHANGED = 3,
            SUPERSEDED = 4,
            CESSATION_OF_OPERATION = 5
        };

        enum class PkiArtifactKind : uint8_t
        {
            CERT = 0,
            TRUST_ANCHOR = 1,
            REVOCATION = 2,
            CHANNEL_BINDING = 3
        };

        enum class DistributionState : uint8_t
        {
            PENDING = 0,
            IN_PROGRESS = 1,
            APPLIED = 2,
            FAILED = 3
        };

        enum class RolloverPhase : uint8_t
        {
            PREPARE = 0,
            DISTRIBUTE = 1,
            ACTIVATE = 2,
            COMPLETE = 3,
            FAILED = 4
        };

        enum class EncryptionAlgorithm : uint8_t
        {
            AES_256_GCM = 0,
            CHACHA20_POLY1305 = 1
        };

        enum class KdfAlgorithm : uint8_t
        {
            PBKDF2_SHA256 = 0,
            ARGON2ID = 1
        };

        enum class KeyRotationPolicy : uint8_t
        {
            MANUAL = 0,
            TIME_BASED = 1,
            USAGE_BASED = 2
        };

        enum class EncryptionKeyStatus : uint8_t
        {
            STAGED = 0,
            ACTIVE = 1,
            RETIRED = 2,
            DESTROYED = 3
        };

        enum class UnlockResult : uint8_t
        {
            NOT_ATTEMPTED = 0,
            SUCCESS = 1,
            FAILED = 2,
            TIMED_OUT = 3
        };

        enum class CryptoProfileId : uint8_t
        {
            MODERN_BASELINE = 0,
            COMPAT_EMULATION = 1,
            MIL_HARDENED = 2
        };

        enum class SecurityTierId : uint8_t
        {
            TIER_0_NONE = 0,
            TIER_1_BASIC = 1,
            TIER_2_STANDARD = 2,
            TIER_3_HARDENED = 3,
            TIER_4_MILITARY_CLUSTER = 4
        };

        enum class KeyProviderKind : uint8_t
        {
            LOCAL_FILE_KEYSTORE = 0,
            OS_KEYRING = 1,
            EXTERNAL_KMS = 2,
            PKCS11_HSM = 3
        };

        struct IndexAccessMethodCatalogInfo
        {
            ID access_method_id;
            std::string method_name;
            std::string index_type_name;
            ID handler_udr_id;
            bool supports_unique = false;
            bool supports_multicolumn = false;
            bool supports_include = false;
            bool supports_partial = false;
            bool supports_order = false;
            bool supports_nulls_order = false;
            bool supports_concurrent = false;
            uint16_t default_fillfactor = 0;
            bool is_system = false;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct IndexOpclassCatalogInfo
        {
            ID opclass_id;
            std::string opclass_name;
            std::string index_type_name;
            ID input_type_id;
            ID collation_id;
            ID owner_schema_id;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct IndexOpclassFunctionCatalogInfo
        {
            ID opclass_fn_id;
            ID opclass_id;
            IndexOpclassFunctionKind fn_kind = IndexOpclassFunctionKind::COMPARE;
            ID function_id;
            uint16_t support_number = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct IndexColumnCatalogInfo
        {
            ID index_column_id;
            ID index_id;
            uint16_t position = 0;
            ID column_id;
            ID expression_sblr_id;
            ID opclass_id;
            ID collation_id;
            IndexSortOrder sort_order = IndexSortOrder::ASC;
            IndexNullOrder null_order = IndexNullOrder::LAST;
            uint16_t prefix_length = 0;
            bool has_prefix_length = false;
            bool is_include = false;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct IndexOptionCatalogInfo
        {
            ID option_id;
            ID index_id;
            std::string option_key;
            std::string option_value;
            IndexOptionValueType option_type = IndexOptionValueType::STRING;
            bool is_valid = true;
            uint64_t updated_time = 0;
        };

        struct IndexMaintenanceCatalogInfo
        {
            ID maintenance_id;
            ID index_id;
            IndexMaintenanceKind maintenance_kind = IndexMaintenanceKind::REBUILD;
            IndexMaintenanceMode maintenance_mode = IndexMaintenanceMode::OFFLINE;
            IndexMaintenanceState maintenance_state = IndexMaintenanceState::BUILDING_SHADOW;
            uint32_t shadow_root_page_id = 0;
            uint32_t shadow_meta_page_id = 0;
            ID target_filespace_id;
            uint16_t target_fillfactor = 0;
            bool has_target_fillfactor = false;
            uint64_t started_txid = 0;
            uint64_t started_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct IndexMaintenanceDeltaCatalogInfo
        {
            ID maintenance_delta_id;
            ID maintenance_id;
            uint64_t delta_id = 0;
            IndexDeltaOp delta_op = IndexDeltaOp::INSERT;
            uint64_t tid_gpid = 0;
            uint16_t tid_slot = 0;
            uint64_t commit_txid = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
        };

        struct IndexBuildDeltaCatalogInfo
        {
            ID build_delta_id;
            ID index_id;
            uint64_t delta_id = 0;
            IndexDeltaOp delta_op = IndexDeltaOp::INSERT;
            uint64_t tid_gpid = 0;
            uint16_t tid_slot = 0;
            ID key_bytes_id;
            uint64_t created_txid = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
        };

        struct IndexPageDeltaCatalogInfo
        {
            ID page_delta_id;
            ID index_id;
            ID target_locality_key_id;
            IndexPageDeltaOp delta_op = IndexPageDeltaOp::INSERT;
            ID logical_row_uuid;
            uint64_t old_tid_gpid = 0;
            uint16_t old_tid_slot = 0;
            bool has_old_tid = false;
            uint64_t new_tid_gpid = 0;
            uint16_t new_tid_slot = 0;
            bool has_new_tid = false;
            ID normalized_key_id;
            ID normalized_old_key_id;
            uint64_t created_xid = 0;
            IndexPageDeltaMergeState merge_state = IndexPageDeltaMergeState::PENDING;
            bool is_valid = true;
            uint64_t created_time = 0;
        };

        struct IndexStatsCatalogInfo
        {
            ID index_id;
            uint32_t stats_version = 0;
            uint64_t last_analyze_txid = 0;
            uint64_t row_count_est = 0;
            uint64_t distinct_count_est = 0;
            float null_frac = 0.0f;
            ID histogram_bounds_id;
            ID most_common_vals_id;
            uint16_t avg_key_len = 0;
            uint16_t avg_entry_len = 0;
            uint32_t leaf_pages = 0;
            uint16_t height = 0;
            uint64_t clustering_factor = 0;
            float correlation = 0.0f;
            float bloat_ratio = 0.0f;
            uint64_t last_vacuum_txid = 0;
            uint64_t last_reindex_txid = 0;
            uint64_t metrics_last_refresh_xid = 0;
            uint32_t family_metrics_version = 0;
            optimizer::IndexFamilyMetricsType family_metrics_type =
                optimizer::IndexFamilyMetricsType::UNKNOWN;
            optimizer::IndexMetricsConfidenceClass metrics_confidence_class =
                optimizer::IndexMetricsConfidenceClass::UNKNOWN;
            optimizer::IndexMetricsQueryabilityState queryability_state =
                optimizer::IndexMetricsQueryabilityState::UNKNOWN;
            std::string family_metrics_payload;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct IndexUsageCatalogInfo
        {
            ID index_id;
            uint64_t scan_count = 0;
            uint64_t tuple_read = 0;
            uint64_t tuple_returned = 0;
            uint64_t index_only_hits = 0;
            uint64_t blocks_read = 0;
            uint64_t blocks_hit = 0;
            uint64_t total_time_ns = 0;
            uint64_t last_used_time = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct IndexContentionCatalogInfo
        {
            ID index_id;
            uint64_t lock_wait_count = 0;
            uint64_t lock_wait_time_ns = 0;
            uint64_t deadlock_count = 0;
            uint64_t latch_wait_count = 0;
            uint64_t latch_wait_time_ns = 0;
            uint64_t unique_key_conflict_count = 0;
            uint64_t hot_key_count = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct IndexStorageCatalogInfo
        {
            ID index_id;
            uint64_t page_count = 0;
            uint64_t bytes_used = 0;
            uint64_t bytes_allocated = 0;
            float fragmentation_ratio = 0.0f;
            ID filespace_id;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct IndexHealthCatalogInfo
        {
            ID index_id;
            uint64_t last_light_scan_txid = 0;
            uint64_t last_light_scan_time = 0;
            IndexHealthStatus light_status = IndexHealthStatus::HEALTHY;
            uint32_t light_error_count = 0;
            uint64_t last_diag_scan_txid = 0;
            uint64_t last_diag_scan_time = 0;
            IndexHealthStatus diagnostic_status = IndexHealthStatus::HEALTHY;
            uint32_t diagnostic_error_count = 0;
            uint32_t checksum_errors = 0;
            uint32_t order_errors = 0;
            uint32_t pointer_errors = 0;
            uint32_t orphan_pages = 0;
            uint32_t duplicate_keys = 0;
            uint32_t in_memory_errors = 0;
            uint64_t pages_scanned = 0;
            uint64_t bytes_scanned = 0;
            uint64_t cleanup_backlog_count = 0;
            uint64_t cleanup_backlog_pages = 0;
            uint64_t cleanup_backlog_bytes = 0;
            uint64_t cleanup_sweep_generation = 0;
            uint64_t cleanup_checkpoint_generation = 0;
            uint64_t cleanup_last_published_time = 0;
            bool cleanup_repair_required = false;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct FilespaceStatsCatalogInfo
        {
            uint32_t filespace_id = 0;
            uint64_t total_pages = 0;
            uint64_t free_pages = 0;
            uint64_t dirty_pages = 0;
            uint64_t read_iops = 0;
            uint64_t write_iops = 0;
            uint64_t last_scan_txid = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct LobCatalogInfo
        {
            ID lob_id;
            ID database_id;
            ID owner_id;
            uint64_t data_length = 0;
            uint64_t page_count = 0;
            bool has_checksum = false;
            uint32_t checksum = 0;
            bool is_encrypted = false;
            ID encryption_key_id;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct LobPageCatalogInfo
        {
            ID lob_page_id;
            ID lob_id;
            uint32_t page_index = 0;
            uint64_t page_gpid = 0;
            uint32_t chunk_bytes = 0;
            bool has_checksum = false;
            uint32_t checksum = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct BackupHistoryCatalogInfo
        {
            ID backup_id;
            ID database_id;
            BackupHistoryKind backup_kind = BackupHistoryKind::FULL;
            BackupHistoryStatus backup_status = BackupHistoryStatus::STARTED;
            std::string storage_profile;
            std::string storage_uri;
            bool has_size_bytes = false;
            uint64_t size_bytes = 0;
            bool has_checksum = false;
            uint32_t checksum = 0;
            uint64_t started_time = 0;
            uint64_t completed_time = 0;
            ID created_by_user_id;
            std::string error_message;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct AuditSinkProfileCatalogInfo
        {
            ID audit_sink_profile_id;
            std::string profile_name;
            std::string sink_type;
            std::string failure_policy;
            std::string config_json;
            bool is_enabled = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct AuditExportSegmentCatalogInfo
        {
            ID audit_export_segment_id;
            ID audit_sink_profile_id;
            std::string evidence_class;
            uint64_t segment_seq = 0;
            uint64_t range_start_time = 0;
            uint64_t range_end_time = 0;
            std::string payload_manifest;
            std::array<uint8_t, 32> hash_prev{};
            std::array<uint8_t, 32> hash_curr{};
            std::string delivery_state;
            bool is_valid = true;
            uint64_t created_time = 0;
        };

        enum class TransactionLineageEventKind : uint8_t
        {
            TX_BEGIN = 1,
            TX_CONTEXT_BOUND = 2,
            TX_MUTATION_BATCH = 3,
            TX_DDL_BATCH = 4,
            TX_COMMIT = 5,
            TX_ROLLBACK = 6,
            TX_ARCHIVE_TRANSFERRED = 7
        };

        struct TransactionLineageEventCatalogInfo
        {
            ID lineage_event_id;
            ID tx_uuid;
            uint64_t txid = 0;
            uint32_t event_seq = 0;
            TransactionLineageEventKind event_kind = TransactionLineageEventKind::TX_BEGIN;
            ID session_id;
            ID connection_id;
            ID user_id;
            ID role_id;
            ID object_id;
            bool has_statement_hash = false;
            uint64_t statement_hash = 0;
            std::string payload_json;
            bool is_valid = true;
            uint64_t created_time = 0;
        };

        struct PageAuditFindingCatalogInfo
        {
            ID finding_id;
            uint64_t finding_time = 0;
            std::string scan_mode;
            std::string trigger_source;
            ID filespace_uuid;
            uint64_t page_id = 0;
            std::string page_type;
            std::string error_code;
            std::string severity;
            ID related_tx_uuid;
            ID related_capsule_uuid;
            std::string details_json;
            bool is_valid = true;
        };

        struct ShadowCaptureManifestCatalogInfo
        {
            ID manifest_id;
            ID tx_uuid;
            ID object_uuid;
            std::string capture_scope;
            std::string capture_format;
            ID sink_profile_uuid;
            std::string payload_manifest;
            uint64_t created_time = 0;
            bool has_retention_deadline_time = false;
            uint64_t retention_deadline_time = 0;
            bool is_valid = true;
        };

        struct SchemaEpochCatalogInfo
        {
            ID schema_epoch_uuid;
            ID database_id;
            bool has_commit_seqno = false;
            uint64_t commit_seqno = 0;
            ID origin_tx_uuid;
            uint64_t origin_txid = 0;
            bool has_parent_schema_epoch_uuid = false;
            ID parent_schema_epoch_uuid;
            std::string definition_manifest;
            uint64_t created_time = 0;
            bool is_valid = true;
        };

        struct SchemaChangePlanCatalogInfo
        {
            ID schema_change_plan_uuid;
            ID object_uuid;
            std::string object_type;
            std::string requested_operation;
            std::string change_class;
            ID requested_by_uuid;
            uint64_t requested_at = 0;
            std::string phase_state;
            uint64_t baseline_schema_epoch = 0;
            bool has_expanded_schema_epoch = false;
            uint64_t expanded_schema_epoch = 0;
            bool has_cutover_schema_epoch = false;
            uint64_t cutover_schema_epoch = 0;
            std::string rollback_class;
            std::string refusal_reason_code;
            bool has_refusal_detail_uuid = false;
            ID refusal_detail_uuid;
            bool is_valid = true;
        };

        struct SchemaChangeEventCatalogInfo
        {
            ID schema_change_event_uuid;
            ID schema_change_plan_uuid;
            uint64_t event_seq = 0;
            bool has_phase_from = false;
            std::string phase_from;
            std::string phase_to;
            std::string event_state;
            std::string event_code;
            bool has_event_detail_uuid = false;
            ID event_detail_uuid;
            uint64_t event_time = 0;
            bool is_valid = true;
        };

        struct SchemaChangeBackfillProgressCatalogInfo
        {
            ID schema_change_backfill_progress_uuid;
            ID schema_change_plan_uuid;
            uint64_t worker_generation = 0;
            uint64_t scanned_row_count = 0;
            uint64_t written_row_count = 0;
            uint64_t validated_row_count = 0;
            bool has_last_resume_row_uuid = false;
            ID last_resume_row_uuid;
            bool has_last_resume_key_json = false;
            std::string last_resume_key_json;
            bool partial_chunk_rewind_required = false;
            std::string restart_disposition;
            bool has_last_heartbeat_at = false;
            uint64_t last_heartbeat_at = 0;
            bool is_valid = true;
        };

        struct SchemaChangeCutoverGuardCatalogInfo
        {
            ID schema_change_cutover_guard_uuid;
            ID schema_change_plan_uuid;
            uint64_t expected_pre_cutover_schema_epoch = 0;
            uint64_t validation_manifest_hash = 0;
            bool dependency_refresh_complete = false;
            bool has_expected_security_epoch = false;
            uint64_t expected_security_epoch = 0;
            std::string guard_state;
            uint64_t checked_at = 0;
            bool is_valid = true;
        };

        struct IndexBuildPlanCatalogInfo
        {
            ID index_build_plan_uuid;
            ID logical_index_id;
            std::string build_reason;
            std::string build_state;
            ID shadow_index_uuid;
            uint64_t baseline_schema_epoch = 0;
            uint64_t build_snapshot_xid = 0;
            bool has_resume_anchor_row_uuid = false;
            ID resume_anchor_row_uuid;
            bool has_resume_payload_json = false;
            std::string resume_payload_json;
            bool is_valid = true;
        };

        struct IndexBuildEventCatalogInfo
        {
            ID index_build_event_uuid;
            ID index_build_plan_uuid;
            uint64_t event_seq = 0;
            bool has_phase_from = false;
            std::string phase_from;
            std::string phase_to;
            std::string event_code;
            uint64_t event_time = 0;
            bool is_valid = true;
        };

        struct IndexBuildProgressCatalogInfo
        {
            ID index_build_progress_uuid;
            ID index_build_plan_uuid;
            uint64_t rows_scanned = 0;
            uint64_t rows_applied = 0;
            uint64_t side_log_records_applied = 0;
            bool has_last_resume_row_uuid = false;
            ID last_resume_row_uuid;
            bool partial_chunk_rewind_required = false;
            std::string restart_disposition;
            bool is_valid = true;
        };

        struct IndexBuildCutoverGuardCatalogInfo
        {
            ID index_build_cutover_guard_uuid;
            ID index_build_plan_uuid;
            uint64_t expected_schema_epoch = 0;
            bool side_log_drained = false;
            uint64_t validation_manifest_hash = 0;
            std::string guard_state;
            uint64_t checked_at = 0;
            bool is_valid = true;
        };

        struct BulkLoadPlanCatalogInfo
        {
            ID bulk_load_plan_uuid;
            ID object_uuid;
            std::string ingest_lane;
            std::string load_kind;
            std::string source_format;
            ID requested_by_uuid;
            uint64_t requested_at = 0;
            std::string phase_state;
            bool has_expected_row_count = false;
            uint64_t expected_row_count = 0;
            bool is_valid = true;
        };

        struct BulkLoadEventCatalogInfo
        {
            ID bulk_load_event_uuid;
            ID bulk_load_plan_uuid;
            uint64_t event_seq = 0;
            bool has_phase_from = false;
            std::string phase_from;
            std::string phase_to;
            std::string event_state;
            std::string event_code;
            uint64_t event_time = 0;
            bool is_valid = true;
        };

        struct BulkLoadProgressCatalogInfo
        {
            ID bulk_load_progress_uuid;
            ID bulk_load_plan_uuid;
            uint64_t worker_generation = 0;
            uint64_t scanned_row_count = 0;
            uint64_t written_row_count = 0;
            uint64_t validated_row_count = 0;
            bool partial_chunk_rewind_required = false;
            std::string restart_disposition;
            bool has_last_heartbeat_at = false;
            uint64_t last_heartbeat_at = 0;
            bool is_valid = true;
        };

        struct BulkLoadCutoverGuardCatalogInfo
        {
            ID bulk_load_cutover_guard_uuid;
            ID bulk_load_plan_uuid;
            uint64_t expected_pre_cutover_schema_epoch = 0;
            uint64_t validation_manifest_hash = 0;
            bool dependency_refresh_complete = false;
            std::string guard_state;
            uint64_t checked_at = 0;
            bool is_valid = true;
        };

        struct MemoryGrantFeedbackCatalogInfo
        {
            ID grant_feedback_uuid;
            uint64_t grant_key_hash = 0;
            ID database_uuid;
            ID schema_root_uuid;
            std::string operator_kind;
            uint64_t sample_count = 0;
            uint64_t last_grant_bytes = 0;
            uint64_t p50_bytes = 0;
            uint64_t p90_bytes = 0;
            uint64_t peak_bytes = 0;
            uint64_t spill_count = 0;
            uint64_t cancel_count = 0;
            uint64_t oscillation_count = 0;
            uint8_t underuse_streak = 0;
            int8_t last_adjustment_direction = 0;
            uint8_t oscillation_disable_count = 0;
            std::string state;
            uint64_t updated_at = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ForensicSnapshotCapsuleCatalogInfo
        {
            ID capsule_id;
            ID database_id;
            ID tx_uuid;
            uint64_t txid = 0;
            bool has_commit_seqno = false;
            uint64_t commit_seqno = 0;
            std::string snapshot_kind;
            ID schema_epoch_uuid;
            std::string active_tx_manifest;
            std::string visibility_manifest;
            ID lineage_root_event_id;
            uint64_t created_time = 0;
            uint64_t retention_deadline_time = 0;
            bool has_archive_locator_uuid = false;
            ID archive_locator_uuid;
            std::string status;
            bool is_valid = true;
        };

        struct RuntimeConnectionCatalogInfo
        {
            ID connection_id;
            ID database_id;
            ID server_instance_id;
            ConnectionTransport transport = ConnectionTransport::LOCAL;
            std::string protocol;
            std::string client_host;
            bool has_client_port = false;
            uint16_t client_port = 0;
            std::string server_host;
            bool has_server_port = false;
            uint16_t server_port = 0;
            bool is_tls = false;
            std::string tls_profile_name;
            std::string client_os;
            std::string client_app;
            std::string client_version;
            std::string client_exec_path;
            bool has_client_pid = false;
            uint32_t client_pid = 0;
            ConnectionAuthMethod auth_method = ConnectionAuthMethod::PASSWORD;
            std::string auth_policy_name;
            ID auth_context_id;
            bool has_route_fingerprint = false;
            uint32_t route_fingerprint = 0;
            ID route_details_id;
            bool is_cluster_bridge = false;
            ID bridge_peer_server_id;
            uint64_t created_time = 0;
            uint64_t last_activity_time = 0;
            bool has_ended_time = false;
            uint64_t ended_time = 0;
            bool is_valid = true;
            uint64_t last_modified_time = 0;
        };

        struct RuntimeTransactionCatalogInfo
        {
            uint64_t txid = 0;
            ID tx_uuid;
            ID database_id;
            ID session_id;
            ID connection_id;
            ID user_id;
            ID role_id;
            EmulationEngine emulation_engine = EmulationEngine::NATIVE;
            uint8_t isolation_level = 0;
            bool read_only = false;
            bool autocommit = false;
            RuntimeTransactionState state = RuntimeTransactionState::IN_PROGRESS;
            uint64_t start_time = 0;
            bool has_end_time = false;
            uint64_t end_time = 0;
            bool has_commit_seqno = false;
            uint64_t commit_seqno = 0;
            ID schema_epoch_uuid;
            ID forensic_snapshot_capsule_uuid;
            bool has_last_statement_hash = false;
            uint64_t last_statement_hash = 0;
            bool has_last_statement_time = false;
            uint64_t last_statement_time = 0;
            bool has_last_error_code = false;
            uint32_t last_error_code = 0;
            std::string last_sqlstate;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct CheckpointRunCatalogInfo
        {
            ID checkpoint_run_uuid;
            uint64_t checkpoint_generation = 0;
            CheckpointLifecycleState checkpoint_state = CheckpointLifecycleState::IDLE;
            uint64_t start_time = 0;
            bool has_end_time = false;
            uint64_t end_time = 0;
            uint64_t dirty_generation_low_watermark = 0;
            uint64_t pages_target = 0;
            uint64_t pages_flushed = 0;
            bool has_failure_reason = false;
            Status failure_reason = Status::OK;
            bool is_valid = true;
        };

        struct RecoveryRunCatalogInfo
        {
            ID recovery_run_uuid;
            uint64_t recovery_generation = 0;
            Database::StartupRecoveryClassification classification =
                Database::StartupRecoveryClassification::NOT_CLASSIFIED;
            uint64_t start_time = 0;
            bool has_end_time = false;
            uint64_t end_time = 0;
            uint64_t normalized_transactions = 0;
            uint64_t repair_required_pages = 0;
            Database::StartupServiceState degraded_state =
                Database::StartupServiceState::NORMAL;
            bool is_valid = true;
        };

        struct SweepCursorStateCatalogInfo
        {
            ID sweep_cursor_state_uuid;
            uint64_t sweep_generation = 0;
            ID relation_uuid;
            ID filespace_uuid;
            uint64_t page_id = 0;
            uint32_t slot_id = 0;
            uint64_t checkpoint_generation_seen = 0;
            uint64_t persist_time = 0;
            bool active = false;
            uint8_t stage = 0;
            uint16_t resume_lane_mask = 0;
            bool resume_strict_audit = true;
            uint64_t start_horizon = 0;
            uint64_t reclaimed_version_count = 0;
            uint64_t reclaimed_bytes = 0;
            uint64_t index_backlog_count = 0;
            uint64_t index_backlog_pages = 0;
            uint64_t index_backlog_bytes = 0;
            uint32_t cursor_crc32c = 0;
            bool is_valid = true;
        };

        struct WritebackIncidentCatalogInfo
        {
            ID writeback_incident_uuid;
            bool has_filespace_uuid = false;
            ID filespace_uuid;
            WritebackQueueKind queue_kind = WritebackQueueKind::UNKNOWN;
            WritebackPolicyDomain policy_domain = WritebackPolicyDomain::UNKNOWN;
            uint64_t page_class = 0;
            WritebackFailureClass failure_class = WritebackFailureClass::NONE;
            uint64_t first_seen_time = 0;
            uint64_t last_seen_time = 0;
            uint64_t retry_count = 0;
            WritebackDegradedState degraded_state = WritebackDegradedState::NORMAL;
            bool has_clearance_condition_uuid = false;
            ID clearance_condition_uuid;
            bool is_open = false;
            bool is_valid = true;
            Status last_error_status = Status::OK;
        };

        struct RecoveryIncidentCatalogInfo
        {
            ID recovery_incident_uuid;
            uint64_t recovery_generation = 0;
            Database::StartupRecoveryClassification classification =
                Database::StartupRecoveryClassification::NOT_CLASSIFIED;
            bool has_checkpoint_generation = false;
            uint64_t checkpoint_generation = 0;
            bool has_object_uuid = false;
            ID object_uuid;
            bool has_details = false;
            std::string details_json;
            uint64_t created_time = 0;
            bool is_valid = true;
        };

        struct PrincipalAccountCatalogInfo
        {
            ID account_id;
            std::string principal_name;
            PrincipalKind principal_kind = PrincipalKind::USER;
            SourceScopeKind source_scope_kind = SourceScopeKind::ANY;
            std::string source_scope_value;
            bool has_source_scope_value = false;
            std::string auth_database;
            bool has_auth_database = false;
            std::string tenant_scope;
            bool has_tenant_scope = false;
            ID auth_policy_id;
            ID password_policy_id;
            bool has_password_policy = false;
            ID default_role_id;
            bool has_default_role = false;
            bool is_login_enabled = true;
            bool no_login_direct = false; // Principal flag: NO_LOGIN_DIRECT
            bool proxy_assertion_only = false;
            std::string proxy_assertion_trust_profile;
            bool has_proxy_assertion_trust_profile = false;
            bool is_locked = false;
            std::string locked_reason;
            bool has_locked_reason = false;
            bool has_valid_from = false;
            uint64_t valid_from_utc = 0;
            bool has_valid_to = false;
            uint64_t valid_to_utc = 0;
            uint64_t created_txid = 0;
            uint64_t last_modified_txid = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct AccountCredentialCatalogInfo
        {
            ID credential_id;
            ID account_id;
            CredentialKind credential_kind = CredentialKind::PASSWORD_ARGON2ID;
            ID credential_payload_id;
            std::array<uint8_t, 32> credential_hash{};
            bool has_credential_hash = false;
            bool has_rotated_time = false;
            uint64_t rotated_time_utc = 0;
            bool has_expires_time = false;
            uint64_t expires_time_utc = 0;
            bool is_active = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct AccountProfileBindingCatalogInfo
        {
            ID binding_id;
            ID account_id;
            ID quota_profile_id;
            bool has_quota_profile = false;
            ID settings_profile_id;
            bool has_settings_profile = false;
            uint16_t priority_u16 = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct PrincipalResolutionRequest
        {
            std::string presented_principal_name;
            std::string source_host;
            std::string source_ip;
            std::string source_socket;
            std::string source_node_id;
            bool has_peer_uid = false;
            uint32_t peer_uid = 0;
            bool has_peer_gid = false;
            uint32_t peer_gid = 0;
            bool has_peer_pid = false;
            uint32_t peer_pid = 0;
            std::string auth_database_context;
            bool has_auth_database_context = false;
            std::string tenant_context;
            bool has_tenant_context = false;
            uint64_t now_utc = 0;
        };

        struct AuthProviderCatalogInfo
        {
            ID provider_id;
            std::string provider_name;
            AuthProviderKind provider_kind = AuthProviderKind::INTERNAL_ARGON2ID;
            AuthProviderState provider_state = AuthProviderState::ENABLED;
            uint16_t priority_rank = 0;
            uint32_t timeout_ms = 0;
            uint32_t cache_ttl_ms = 0;
            AuthProviderFailMode fail_mode = AuthProviderFailMode::HARD_FAIL;
            std::string config_payload;
            std::vector<std::string> proxy_assertion_allowed_signers;
            std::vector<std::string> allowed_plugins;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        // Auth policy method mask bits for protocol negotiation.
        static constexpr uint16_t AUTH_POLICY_METHOD_PASSWORD       = 1u << 0;
        static constexpr uint16_t AUTH_POLICY_METHOD_MD5            = 1u << 1;
        static constexpr uint16_t AUTH_POLICY_METHOD_SCRAM_SHA_256  = 1u << 2;
        static constexpr uint16_t AUTH_POLICY_METHOD_SCRAM_SHA_512  = 1u << 3;
        static constexpr uint16_t AUTH_POLICY_METHOD_TOKEN          = 1u << 4;
        static constexpr uint16_t AUTH_POLICY_METHOD_PEER           = 1u << 5;
        static constexpr uint16_t AUTH_POLICY_METHOD_PLUGIN_REGISTRY = 1u << 6;
        static constexpr uint32_t AUTH_POLICY_MIN_SCRAM_ITERATIONS_DEFAULT = 4096;
        static constexpr uint16_t AUTH_POLICY_METHOD_ALL =
            AUTH_POLICY_METHOD_PASSWORD |
            AUTH_POLICY_METHOD_MD5 |
            AUTH_POLICY_METHOD_SCRAM_SHA_256 |
            AUTH_POLICY_METHOD_SCRAM_SHA_512 |
            AUTH_POLICY_METHOD_TOKEN |
            AUTH_POLICY_METHOD_PEER |
            AUTH_POLICY_METHOD_PLUGIN_REGISTRY;

        // Auth policy transport mask bits for protocol negotiation.
        static constexpr uint8_t AUTH_POLICY_TRANSPORT_LOCAL = 1u << 0;
        static constexpr uint8_t AUTH_POLICY_TRANSPORT_IPC   = 1u << 1;
        static constexpr uint8_t AUTH_POLICY_TRANSPORT_INET  = 1u << 2;
        static constexpr uint8_t AUTH_POLICY_TRANSPORT_ALL =
            AUTH_POLICY_TRANSPORT_LOCAL |
            AUTH_POLICY_TRANSPORT_IPC |
            AUTH_POLICY_TRANSPORT_INET;

        struct AuthPolicyCatalogInfo
        {
            ID policy_id;
            std::string policy_name;
            std::vector<ID> provider_chain;
            bool mfa_required = false;
            bool has_mfa_policy = false;
            ID mfa_policy_id;
            uint16_t lockout_threshold = 0;
            uint32_t lockout_window_ms = 0;
            uint32_t lockout_duration_ms = 0;
            bool allow_password_fallback = false;
            uint16_t allowed_auth_method_mask = AUTH_POLICY_METHOD_ALL;
            std::vector<std::string> allowed_auth_method_ids;
            std::vector<std::string> client_pinning_required_methods;
            std::vector<std::string> client_pinning_forbidden_methods;
            bool client_pinning_require_channel_binding = false;
            bool has_required_auth_method = false;
            ConnectionAuthMethod required_auth_method = ConnectionAuthMethod::SCRAM_SHA_256;
            uint8_t allowed_transport_mask = AUTH_POLICY_TRANSPORT_ALL;
            AuthPeerMode peer_mode = AuthPeerMode::DISABLED;
            uint32_t min_scram_iterations = AUTH_POLICY_MIN_SCRAM_ITERATIONS_DEFAULT;
            bool mark_weak_scram_for_upgrade = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct MfaPolicyCatalogInfo
        {
            ID mfa_policy_id;
            std::string policy_name;
            MfaFactorType primary_factor = MfaFactorType::TOTP;
            bool allow_recovery_codes = true;
            bool allow_break_glass = false;
            uint8_t max_challenge_attempts = 3;
            uint32_t challenge_ttl_ms = 300000;
            uint32_t step_up_ttl_ms = 900000;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct MfaEnrollmentCatalogInfo
        {
            ID enrollment_id;
            ID account_id;
            ID mfa_policy_id;
            MfaFactorType factor_type = MfaFactorType::TOTP;
            bool is_primary = true;
            bool is_enrolled = true;
            bool has_secret = false;
            std::string secret_base32;
            uint8_t totp_digits = 6;
            uint32_t totp_period = 30;
            uint32_t totp_look_ahead = 1;
            uint32_t totp_look_behind = 1;
            uint64_t enrolled_time_utc = 0;
            bool has_last_verified_time = false;
            uint64_t last_verified_time_utc = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct MfaRecoveryCodeCatalogInfo
        {
            ID recovery_id;
            ID account_id;
            ID mfa_policy_id;
            bool is_break_glass = false;
            std::array<uint8_t, 32> code_hash{};
            uint32_t max_uses = 1;
            uint32_t uses = 0;
            uint32_t cooldown_ms = 0;
            bool has_last_used_time = false;
            uint64_t last_used_time_utc = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct AuthAttemptLogCatalogInfo
        {
            ID attempt_id;
            ID connection_id;
            bool has_account_id = false;
            ID account_id;
            bool has_provider_id = false;
            ID provider_id;
            AuthAttemptOutcome outcome = AuthAttemptOutcome::FAIL;
            std::string failure_code;
            bool has_failure_code = false;
            uint64_t attempt_time_utc = 0;
            uint64_t latency_us = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ConnectionRuleCatalogInfo
        {
            ID rule_id;
            std::string profile_scope;
            uint32_t rule_order = 0;
            ConnectionRuleTransportKind transport_kind = ConnectionRuleTransportKind::TCP;
            bool has_source_cidr = false;
            std::string source_cidr;
            bool has_source_host_pattern = false;
            std::string source_host_pattern;
            bool has_principal_pattern = false;
            std::string principal_pattern;
            bool has_auth_database_pattern = false;
            std::string auth_database_pattern;
            bool has_tenant_pattern = false;
            std::string tenant_pattern;
            bool has_target_db_pattern = false;
            std::string target_db_pattern;
            bool has_required_provider_kind = false;
            AuthProviderKind required_provider_kind = AuthProviderKind::INTERNAL_ARGON2ID;
            bool has_required_tls_mode = false;
            ConnectionRuleTlsMode required_tls_mode = ConnectionRuleTlsMode::NONE;
            ConnectionRuleAction action = ConnectionRuleAction::DENY;
            bool has_mapped_auth_policy = false;
            ID mapped_auth_policy_id;
            bool has_mapped_default_role = false;
            ID mapped_default_role_id;
            bool has_reject_code_override = false;
            std::string reject_code_override;
            bool is_enabled = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool has_expected_epoch = false;
            uint64_t expected_epoch_u64 = 0;
        };

        struct ConnectionRuleEpochCatalogInfo
        {
            std::string profile_scope;
            uint64_t rule_epoch_u64 = 0;
            uint64_t last_modified_utc = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ConnectionRuleEvaluationRequest
        {
            std::string profile_scope;
            ConnectionRuleTransportKind transport_kind = ConnectionRuleTransportKind::TCP;
            std::string source_ip;
            std::string source_host;
            std::string source_socket;
            std::string principal_name;
            std::string auth_database;
            std::string tenant;
            std::string target_db;
            std::string remote_address;
            bool has_forwarded_identity = false;
            bool trusted_proxy_channel = false;
            std::string proxy_identity;
            bool has_provider_kind = false;
            AuthProviderKind provider_kind = AuthProviderKind::INTERNAL_ARGON2ID;
        };

        struct ConnectionRuleEvaluationDecision
        {
            bool matched = false;
            ID matched_rule_id;
            ConnectionRuleAction action = ConnectionRuleAction::DENY;
            bool has_mapped_auth_policy = false;
            ID mapped_auth_policy_id;
            bool has_mapped_default_role = false;
            ID mapped_default_role_id;
            bool has_required_provider_kind = false;
            AuthProviderKind required_provider_kind = AuthProviderKind::INTERNAL_ARGON2ID;
            bool has_required_tls_mode = false;
            ConnectionRuleTlsMode required_tls_mode = ConnectionRuleTlsMode::NONE;
            std::string reject_code;
        };

        struct AuthProviderAdapterResult
        {
            ID provider_id;
            AuthAdapterOutcome outcome = AuthAdapterOutcome::REJECT;
        };

        struct AuthProviderRuntimeRequest
        {
            ID account_id;
            ID connection_id;
            std::vector<CredentialKind> credential_kinds;
            std::vector<AuthProviderKind> client_capabilities;
            bool has_required_provider_kind = false;
            AuthProviderKind required_provider_kind = AuthProviderKind::INTERNAL_ARGON2ID;
            std::vector<AuthProviderAdapterResult> adapter_results;
            bool mfa_completed = true;
            uint64_t now_utc = 0;
        };

        struct AuthProviderRuntimeDecision
        {
            bool success = false;
            bool lockout_applied = false;
            bool policy_requires_mfa = false;
            ID selected_provider_id;
            std::vector<ID> attempted_provider_ids;
            std::string reject_code;
        };

        struct EffectiveSubjectRef
        {
            ID subject_id;
            AuthorizationSubjectType subject_type = AuthorizationSubjectType::USER;
        };

        struct AclCommandCatalogInfo
        {
            std::string command_name;
            uint64_t category_bits = 0;
            AclCommandArityClass arity_class = AclCommandArityClass::FIXED;
            bool is_write = false;
            bool is_admin = false;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct AclRuleCatalogInfo
        {
            ID acl_rule_id;
            ID subject_id;
            AuthorizationSubjectType subject_type = AuthorizationSubjectType::USER;
            PolicyEffect effect = PolicyEffect::ALLOW;
            std::string command_pattern;
            bool has_category_mask = false;
            uint64_t category_mask = 0;
            bool has_key_pattern = false;
            std::string key_pattern;
            bool has_channel_pattern = false;
            std::string channel_pattern;
            uint16_t priority_u16 = 0;
            bool is_enabled = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct AclEvaluationRequest
        {
            std::string command_name;
            uint64_t command_category_bits = 0;
            std::vector<std::string> keys;
            std::vector<std::string> channels;
            std::vector<EffectiveSubjectRef> effective_subjects;
        };

        struct AclEvaluationDecision
        {
            bool matched = false;
            bool allowed = false;
            ID matched_rule_id;
            std::string reject_code;
        };

        struct DocumentPolicyCatalogInfo
        {
            ID policy_id;
            DocumentEngineTag engine_tag = DocumentEngineTag::GENERIC_DOC;
            std::string resource_pattern;
            bool has_tenant_pattern = false;
            std::string tenant_pattern;
            PolicyEffect effect = PolicyEffect::ALLOW;
            bool has_doc_filter_sblr_uuid = false;
            ID doc_filter_sblr_uuid;
            bool has_write_filter_sblr_uuid = false;
            ID write_filter_sblr_uuid;
            std::vector<std::string> field_allowlist;
            std::vector<std::string> field_denylist;
            uint16_t priority_u16 = 0;
            bool is_enabled = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct TenantBindingCatalogInfo
        {
            ID binding_id;
            ID subject_id;
            AuthorizationSubjectType subject_type = AuthorizationSubjectType::USER;
            std::string tenant_name;
            uint16_t priority_u16 = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct DocumentAuthorizationRequest
        {
            DocumentEngineTag engine_tag = DocumentEngineTag::GENERIC_DOC;
            std::string resource_name;
            std::string tenant_name;
            std::vector<EffectiveSubjectRef> effective_subjects;
            std::vector<std::string> candidate_fields;
            std::vector<ID> predicate_matched_policy_ids;
            bool require_model_policy = false;
            bool require_non_empty_document = false;
            bool is_write = false;
        };

        struct DocumentAuthorizationDecision
        {
            bool allowed = false;
            bool filtered_to_empty = false;
            std::vector<std::string> projected_fields;
            std::vector<ID> matched_policy_ids;
            std::string reject_code;
        };

        struct GraphPrivilegeCatalogInfo
        {
            ID graph_priv_id;
            ID subject_id;
            AuthorizationSubjectType subject_type = AuthorizationSubjectType::USER;
            std::string graph_scope;
            bool has_label_pattern = false;
            std::string label_pattern;
            bool has_relationship_pattern = false;
            std::string relationship_pattern;
            bool has_property_pattern = false;
            std::string property_pattern;
            uint64_t action_bits = 0;
            PolicyEffect effect = PolicyEffect::ALLOW;
            uint16_t priority_u16 = 0;
            bool is_enabled = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct GraphAuthorizationRequest
        {
            std::string graph_scope;
            std::string label_name;
            std::string relationship_name;
            std::string property_name;
            uint64_t requested_action_bits = 0;
            std::vector<EffectiveSubjectRef> effective_subjects;
        };

        struct GraphAuthorizationDecision
        {
            bool allowed = false;
            ID matched_rule_id;
            std::string reject_code;
        };

        struct TokenCatalogInfo
        {
            ID token_id;
            TokenKind token_kind = TokenKind::BEARER;
            std::array<uint8_t, 32> token_hash{};
            std::string issuer;
            ID subject_account_id;
            TokenScopeModel scope_model = TokenScopeModel::GENERIC;
            uint64_t not_before_utc = 0;
            uint64_t not_after_utc = 0;
            bool has_revoked_time_utc = false;
            uint64_t revoked_time_utc = 0;
            bool has_rotation_group = false;
            ID rotation_group_id;
            bool has_last_used_time_utc = false;
            uint64_t last_used_time_utc = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct TokenScopeEntryCatalogInfo
        {
            ID scope_id;
            ID token_id;
            PolicyEffect effect = PolicyEffect::ALLOW;
            TokenResourceKind resource_kind = TokenResourceKind::DATABASE;
            std::string resource_pattern;
            uint64_t action_bits = 0;
            uint16_t priority_u16 = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct TokenValidationRequest
        {
            std::array<uint8_t, 32> presented_token_hash{};
            TokenResourceKind resource_kind = TokenResourceKind::DATABASE;
            std::string resource_name;
            uint64_t requested_action_bits = 0;
            uint64_t now_utc = 0;
        };

        struct TokenValidationDecision
        {
            bool allowed = false;
            ID token_id;
            ID subject_account_id;
            TokenScopeModel scope_model = TokenScopeModel::GENERIC;
            std::vector<ID> matched_scope_ids;
            std::string reject_code;
        };

        struct QuotaProfileCatalogInfo
        {
            ID quota_profile_id;
            std::string profile_name;
            uint32_t max_requests_per_sec = 0;
            uint32_t max_concurrent_requests = 0;
            uint64_t max_read_bytes_per_sec = 0;
            uint64_t max_write_bytes_per_sec = 0;
            uint64_t max_result_rows = 0;
            uint64_t max_cpu_ms_per_min = 0;
            uint32_t window_ms = 0;
            bool is_enabled = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct QuotaBindingCatalogInfo
        {
            ID binding_id;
            BindingSubjectType subject_type = BindingSubjectType::GLOBAL;
            bool has_subject_id = false;
            ID subject_id;
            bool has_tenant_scope = false;
            std::string tenant_scope;
            BindingResourceScopeKind resource_scope_kind = BindingResourceScopeKind::GLOBAL;
            bool has_resource_scope_value = false;
            std::string resource_scope_value;
            ID quota_profile_id;
            uint16_t priority_u16 = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct SettingsProfileCatalogInfo
        {
            ID settings_profile_id;
            std::string profile_name;
            std::string settings_payload;
            bool strict_mode = false;
            bool is_enabled = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct SettingsBindingCatalogInfo
        {
            ID binding_id;
            BindingSubjectType subject_type = BindingSubjectType::GLOBAL;
            bool has_subject_id = false;
            ID subject_id;
            bool has_tenant_scope = false;
            std::string tenant_scope;
            BindingResourceScopeKind resource_scope_kind = BindingResourceScopeKind::GLOBAL;
            bool has_resource_scope_value = false;
            std::string resource_scope_value;
            ID settings_profile_id;
            uint16_t priority_u16 = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        enum class ConfigValueSource : uint8_t
        {
            CATALOG = 1,
            BOOTSTRAP = 2,
            SESSION_OVERRIDE = 3,
        };

        struct ConfigKeyCatalogInfo
        {
            uint32_t key_id = 0;
            std::string key_name;
            config::CatalogValueType value_type = config::CatalogValueType::STRING;
            config::CatalogScope scope = config::CatalogScope::INSTANCE;
            std::string default_value;
            std::string min_value;
            std::string max_value;
            std::string allowed_values;
            bool is_restart_required = false;
            bool is_mutable = true;
            bool is_bootstrap_only = false;
            bool is_cluster_managed = false;
            config::CatalogHotApplyClass hot_apply_class = config::CatalogHotApplyClass::NONE;
            bool is_sensitive = false;
            std::string description;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ConfigValueCatalogInfo
        {
            ID config_value_id;
            uint32_t key_id = 0;
            bool has_scope_uuid = false;
            ID scope_uuid;
            std::string value_text;
            ConfigValueSource source = ConfigValueSource::CATALOG;
            uint64_t config_generation = 0;
            uint64_t effective_txid = 0;
            bool pending_restart = false;
            ID updated_by;
            uint64_t updated_at = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ConfigChangeLogCatalogInfo
        {
            uint64_t change_id = 0;
            uint32_t key_id = 0;
            bool has_scope_uuid = false;
            ID scope_uuid;
            std::string old_value_text;
            std::string new_value_text;
            std::string change_reason;
            uint64_t config_generation = 0;
            ID changed_by;
            uint64_t changed_at = 0;
            bool is_valid = true;
        };

        struct ListenerProfileCatalogInfo
        {
            ID listener_profile_id;
            std::string profile_name;
            std::string protocol_family;
            bool enabled = true;
            bool manager_fronted = false;
            bool has_owner_database_uuid = false;
            ID owner_database_uuid;
            std::string desired_state = "ENABLED";
            uint64_t applied_generation = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
        };

        struct ListenerBindingCatalogInfo
        {
            ID listener_binding_id;
            ID listener_profile_id;
            std::string bind_address;
            uint16_t bind_port = 0;
            std::string bind_transport = "inet";
            std::string bind_scope = "global";
            bool is_primary = true;
            uint64_t configuration_generation = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ListenerEmulationBindingCatalogInfo
        {
            ID listener_emulation_binding_id;
            ID listener_profile_id;
            std::string emulation_family;
            std::string protocol_surface;
            bool enabled = true;
            bool has_parser_pool_policy_uuid = false;
            ID parser_pool_policy_uuid;
            uint64_t configuration_generation = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ParserPoolPolicyCatalogInfo
        {
            ID parser_pool_policy_id;
            std::string policy_name;
            std::string parser_library_family;
            uint16_t min_workers = 0;
            uint16_t preferred_workers = 0;
            uint16_t max_workers = 0;
            uint16_t queue_max = 0;
            uint64_t queue_timeout_ms = 0;
            uint64_t idle_timeout_ms = 0;
            uint64_t spawn_backoff_ms = 0;
            uint64_t health_interval_ms = 0;
            uint16_t missed_heartbeat_threshold = 0;
            uint64_t warm_replenish_timeout_ms = 0;
            uint64_t memory_guardrail_bytes = 0;
            std::string workload_guardrail_class;
            uint64_t configuration_generation = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ListenerRuntimeTargetCatalogInfo
        {
            ID listener_runtime_target_id;
            ID listener_profile_id;
            std::string target_kind;
            bool has_target_database_uuid = false;
            ID target_database_uuid;
            bool has_target_server_uuid = false;
            ID target_server_uuid;
            bool has_inner_listener_profile_uuid = false;
            ID inner_listener_profile_uuid;
            uint64_t current_generation = 0;
            bool has_pending_generation = false;
            uint64_t pending_generation = 0;
            bool has_last_applied_generation = false;
            uint64_t last_applied_generation = 0;
            bool has_last_refused_generation = false;
            uint64_t last_refused_generation = 0;
            std::string last_error_code;
            bool has_last_error_detail_uuid = false;
            ID last_error_detail_uuid;
            uint64_t last_observed_at = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ListenerGenerationRecordCatalogInfo
        {
            ID listener_generation_id;
            ID target_database_uuid;
            ID listener_profile_id;
            uint64_t committed_generation = 0;
            uint64_t applied_generation = 0;
            bool has_refused_generation = false;
            uint64_t refused_generation = 0;
            std::string drift_state = "CONSISTENT";
            bool has_last_instruction_uuid = false;
            ID last_instruction_uuid;
            uint64_t observed_at = 0;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct ProfileResolutionRequest
        {
            bool has_user_id = false;
            ID user_id;
            std::vector<ID> role_ids;
            std::vector<ID> group_ids;
            std::string tenant_scope;
            bool has_database_scope = false;
            ID database_scope_id;
            std::string schema_scope_name;
            std::string resource_scope_name;
        };

        struct QuotaEvaluationRequest
        {
            ProfileResolutionRequest profile_context;
            bool require_profile = false;
            uint32_t concurrent_requests = 0;
            uint32_t window_request_count = 0;
            uint64_t window_read_bytes = 0;
            uint64_t window_write_bytes = 0;
            uint64_t estimated_result_rows = 0;
            uint64_t estimated_cpu_ms = 0;
        };

        struct QuotaEvaluationDecision
        {
            bool allowed = false;
            bool profile_resolved = false;
            ID quota_profile_id;
            std::string profile_name;
            std::string reject_code;
        };

        struct SettingsResolutionRequest
        {
            ProfileResolutionRequest profile_context;
            bool require_profile = false;
            std::unordered_map<std::string, std::string> session_overrides;
        };

        struct SettingsResolutionDecision
        {
            bool applied = false;
            bool profile_resolved = false;
            ID settings_profile_id;
            std::string profile_name;
            std::unordered_map<std::string, std::string> merged_settings;
            std::string reject_code;
        };

        struct AuthMappingCatalogInfo
        {
            ID mapping_id;
            AuthMethod auth_method = AuthMethod::LDAP;
            std::string auth_source;
            std::string external_subject;
            std::string external_group;
            ID database_id;
            ID user_id;
            ID role_id;
            ID group_id;
            uint8_t priority = 0;
            bool is_enabled = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct RoleSettingCatalogInfo
        {
            ID role_setting_id;
            ID role_id;
            ID database_id;
            std::string setting_key;
            std::string setting_value;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t modified_time = 0;
        };

        struct SecurityLabelCatalogInfo
        {
            ID security_label_id;
            ID object_id;
            std::string provider_name;
            std::string label_text;
            ID created_by_id;
            bool is_valid = true;
            uint64_t created_time = 0;
        };

        struct SecurityClassCatalogInfo
        {
            ID security_class_id;
            std::string class_name;
            ID acl_payload_id;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t modified_time = 0;
        };

        struct CertRegistryCatalogInfo
        {
            ID cert_id;
            CertKind cert_kind = CertKind::SERVER;
            std::string subject_name;
            std::string issuer_name;
            std::string serial_number;
            uint64_t not_before = 0;
            uint64_t not_after = 0;
            ID public_key_id;
            ID cert_der_id;
            ID cert_pem_id;
            std::string signature_algorithm;
            std::array<uint8_t, 32> thumbprint_sha256{};
            CertStatus status = CertStatus::ACTIVE;
            uint64_t created_time = 0;
            uint64_t revoked_time = 0;
            bool has_revoked_time = false;
            bool is_valid = true;
        };

        struct PrivateKeyStoreCatalogInfo
        {
            ID key_id;
            ID cert_id;
            KeyMaterialKind key_kind = KeyMaterialKind::ASYMMETRIC_PRIVATE;
            ID key_material_encrypted_id;
            ID kek_profile_id;
            uint64_t created_time = 0;
            uint64_t rotated_time = 0;
            bool has_rotated_time = false;
            uint64_t destroyed_time = 0;
            bool has_destroyed_time = false;
            bool is_valid = true;
        };

        struct TrustAnchorCatalogInfo
        {
            ID anchor_id;
            ID cert_id;
            std::array<uint8_t, 32> thumbprint_sha256{};
            TrustAnchorState state = TrustAnchorState::ACTIVE;
            uint64_t activated_time = 0;
            uint64_t expires_time = 0;
            bool has_expires_time = false;
            ID rollover_group_id;
            bool is_valid = true;
        };

        struct ChannelCertBindingCatalogInfo
        {
            ID binding_id;
            std::string channel_name;
            CertKind cert_kind = CertKind::SERVER;
            ID cert_id;
            ID encryption_profile_id;
            bool enforce_mtls = false;
            TlsVersion min_tls_version = TlsVersion::TLS_1_2;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct CertRevocationCatalogInfo
        {
            ID revocation_id;
            ID cert_id;
            RevocationSource source_kind = RevocationSource::LOCAL;
            RevocationReason reason_code = RevocationReason::UNSPECIFIED;
            uint64_t revoked_time = 0;
            uint64_t expiry_time = 0;
            bool has_expiry_time = false;
            ID evidence_id;
            uint64_t created_time = 0;
            bool is_valid = true;
        };

        struct PkiDistributionStateCatalogInfo
        {
            ID distribution_id;
            ID member_id;
            PkiArtifactKind artifact_kind = PkiArtifactKind::CERT;
            ID artifact_id;
            std::array<uint8_t, 32> artifact_hash{};
            DistributionState distribution_state = DistributionState::PENDING;
            uint32_t retry_count = 0;
            uint64_t last_attempt_time = 0;
            bool has_last_attempt_time = false;
            uint64_t last_success_time = 0;
            bool has_last_success_time = false;
            ID last_error_id;
            bool is_valid = true;
        };

        struct TrustAnchorRolloverCatalogInfo
        {
            ID rollover_id;
            ID rollover_group_id;
            ID old_anchor_id;
            ID new_anchor_id;
            RolloverPhase phase = RolloverPhase::PREPARE;
            uint16_t quorum_required = 0;
            uint16_t quorum_acked = 0;
            uint64_t started_time = 0;
            uint64_t completed_time = 0;
            bool has_completed_time = false;
            uint64_t deadline_time = 0;
            bool has_deadline_time = false;
            ID last_error_id;
            bool is_valid = true;
        };

        enum class ClockSourceKind : uint8_t
        {
            NTP = 0,
            PTP = 1,
            PEER_MEDIAN = 2
        };

        enum class ClusterNodeState : uint8_t
        {
            JOINING = 0,
            SYNCING = 1,
            WARMING = 2,
            ONLINE = 3,
            DRAINING = 4,
            OFFLINE = 5,
            SUSPECT = 6,
            FAILED = 7
        };

        enum class ClusterNodeRole : uint8_t
        {
            METADATA = 0,
            OLTP_DATA = 1,
            ROUTER = 2,
            PARSER = 3,
            LISTENER = 4,
            BACKUP = 5,
            SCHEDULER = 6,
            METRICS = 7,
            OLAP_INGEST = 8,
            OLAP_STORAGE = 9,
            OLAP_COMPUTE = 10,
            VECTOR_INDEX = 11,
            SEARCH_INDEX = 12,
            GRAPH_COMPUTE = 13,
            CACHE = 14
        };

        enum class ClusterServiceType : uint8_t
        {
            OLTP_RPC = 0,
            OLAP_INGEST = 1,
            OLAP_QUERY = 2,
            VECTOR_QUERY = 3,
            TEXT_SEARCH = 4,
            GRAPH_QUERY = 5,
            BACKUP = 6,
            METRICS = 7,
            ADMIN = 8
        };

        enum class ClusterServiceState : uint8_t
        {
            STARTING = 0,
            ONLINE = 1,
            DRAINING = 2,
            OFFLINE = 3
        };

        struct NodeCatalogInfo
        {
            ID node_id;
            ID cluster_id;
            std::string node_name;
            ClusterNodeRole node_role = ClusterNodeRole::METADATA;
            std::string host;
            uint16_t port = 0;
            ConnectionTransport transport = ConnectionTransport::LOCAL;
            std::string region;
            std::string zone;
            std::string rack;
            ClusterNodeState state = ClusterNodeState::JOINING;
            bool has_last_heartbeat_time = false;
            uint64_t last_heartbeat_time = 0;
            uint64_t created_txid = 0;
            uint64_t last_modified_txid = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct NodeRoleBindingCatalogInfo
        {
            ID binding_id;
            ID node_id;
            ClusterNodeRole role = ClusterNodeRole::METADATA;
            bool is_primary = false;
            uint64_t created_txid = 0;
            uint64_t last_modified_txid = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct NodeServiceCatalogInfo
        {
            ID service_id;
            ID node_id;
            ClusterNodeRole role = ClusterNodeRole::METADATA;
            ClusterServiceType service_type = ClusterServiceType::OLTP_RPC;
            ConnectionTransport transport = ConnectionTransport::LOCAL;
            std::string host;
            uint16_t port = 0;
            ID tls_profile_id;
            ID auth_profile_id;
            ClusterServiceState state = ClusterServiceState::STARTING;
            uint64_t created_txid = 0;
            uint64_t last_modified_txid = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct NodeCapabilityCatalogInfo
        {
            ID capability_id;
            ID node_id;
            std::string capability_key;
            std::string capability_value;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        enum class ClusterMode : uint8_t
        {
            WORKGROUP = 0,
            CLUSTER = 1
        };

        enum class ClusterState : uint8_t
        {
            INIT = 0,
            ONLINE = 1,
            DEGRADED = 2,
            MAINTENANCE = 3,
            OFFLINE = 4
        };

        enum class ConsensusMode : uint8_t
        {
            SINGLE = 0,
            RAFT = 1,
            PAXOS = 2
        };

        enum class ConsistencyLevel : uint8_t
        {
            ONE = 0,
            QUORUM = 1,
            ALL = 2,
            LOCAL_QUORUM = 3
        };

        enum class FailoverMode : uint8_t
        {
            DISABLED = 0,
            MANUAL = 1,
            AUTO = 2
        };

        enum class RebalanceMode : uint8_t
        {
            MANUAL = 0,
            AUTO = 1,
            SCHEDULED = 2
        };

        enum class ShardPolicyParamType : uint8_t
        {
            U64 = 0,
            I64 = 1,
            F64 = 2,
            BOOL = 3,
            TEXT = 4,
            UUID = 5,
            JSON = 6
        };

        enum class ShardKeyKind : uint8_t
        {
            HASH = 0,
            RANGE = 1,
            GEO = 2,
            TOKEN = 3
        };

        enum class HashFunctionKind : uint8_t
        {
            MURMUR3 = 0,
            XXHASH64 = 1,
            SHA256 = 2
        };

        enum class RangeOrderKind : uint8_t
        {
            ASC = 0,
            DESC = 1
        };

        enum class ShardState : uint8_t
        {
            CREATING = 0,
            ONLINE = 1,
            REBALANCING = 2,
            DRAINING = 3,
            OFFLINE = 4
        };

        enum class ShardKind : uint8_t
        {
            ROW = 0,
            COLUMN = 1,
            VECTOR = 2,
            DOCUMENT = 3
        };

        enum class ShardRangeKind : uint8_t
        {
            TOKEN = 0,
            HASH_BUCKET = 1,
            BYTES = 2,
            GEO = 3
        };

        enum class ReplicaRole : uint8_t
        {
            PRIMARY = 0,
            SECONDARY = 1,
            LEARNER = 2
        };

        enum class ReplicaState : uint8_t
        {
            SYNCING = 0,
            ONLINE = 1,
            LAGGING = 2,
            OFFLINE = 3
        };

        enum class ShardMigrationState : uint8_t
        {
            PLANNED = 0,
            RUNNING = 1,
            PAUSED = 2,
            COMPLETED = 3,
            FAILED = 4
        };

        enum class ThrottleState : uint8_t
        {
            NONE = 0,
            LOW = 1,
            MEDIUM = 2,
            HIGH = 3
        };

        enum class WorkloadMatchKind : uint8_t
        {
            ROLE = 0,
            USER = 1,
            DATABASE = 2,
            SCHEMA = 3,
            CLIENT_APP = 4,
            STATEMENT_TAG = 5,
            QUERY_TYPE = 6,
            REGEX = 7,
            RESOURCE_TAG = 8,
            CUSTOM = 9
        };

        enum class RouteTargetKind : uint8_t
        {
            NODE = 0,
            SERVICE = 1,
            ROLE = 2,
            SHARD = 3,
            TIER = 4
        };

        enum class AdmissionRejectMode : uint8_t
        {
            REJECT = 0,
            QUEUE = 1,
            SHED_LOW_PRIORITY = 2
        };

        enum class AdmissionTargetKind : uint8_t
        {
            CLUSTER = 0,
            NODE = 1,
            SERVICE = 2,
            WORKLOAD_CLASS = 3
        };

        enum class SloBurnSeverity : uint8_t
        {
            NONE = 0,
            MODERATE = 1,
            HIGH = 2,
            CRITICAL = 3
        };

        enum class SloActionPlan : uint8_t
        {
            NONE = 0,
            ADMISSION_TIGHTEN = 1,
            SCALE_OUT = 2,
            SCALE_OUT_AND_TIGHTEN = 3,
            INCIDENT_PAGE = 4
        };

        enum class AutoscaleActionKind : uint8_t
        {
            SCALE_OUT = 0,
            SCALE_IN = 1,
            NO_OP = 2
        };

        enum class AutoscaleActionState : uint8_t
        {
            PENDING = 0,
            APPLIED = 1,
            FAILED = 2,
            CANCELLED = 3
        };

        enum class ClusterPolicyKind : uint8_t
        {
            BASE = 0,
            SECURITY = 1,
            ROUTING = 2,
            HEALING = 3,
            CUSTOM = 4
        };

        enum class FailureDetectorKind : uint8_t
        {
            PHI = 0,
            HEARTBEAT = 1,
            GOSSIP = 2
        };

        enum class AlertRuleKind : uint8_t
        {
            METRIC = 0,
            EVENT = 1,
            LOG = 2
        };

        enum class AlertSeverity : uint8_t
        {
            INFO = 0,
            WARNING = 1,
            CRITICAL = 2
        };

        enum class AlertTargetKind : uint8_t
        {
            EMAIL = 0,
            WEBHOOK = 1,
            SYSLOG = 2,
            PAGER = 3,
            SMS = 4,
            SLACK = 5,
            CUSTOM = 6
        };

        enum class AlertRouteKind : uint8_t
        {
            IMMEDIATE = 0,
            BATCH = 1,
            ESCALATION = 2
        };

        enum class AlertEventState : uint8_t
        {
            OPEN = 0,
            ACKED = 1,
            RESOLVED = 2,
            SUPPRESSED = 3
        };

        enum class AlertSilenceScope : uint8_t
        {
            CLUSTER = 0,
            NODE = 1,
            RULE = 2,
            TARGET = 3
        };

        enum class PartitionState : uint8_t
        {
            OPEN = 0,
            RESOLVED = 1
        };

        enum class HealingTriggerKind : uint8_t
        {
            PARTITION = 0,
            FAILOVER = 1,
            CAPACITY = 2,
            MANUAL = 3
        };

        enum class HealingActionKind : uint8_t
        {
            RESTART_NODE = 0,
            REBALANCE_SHARDS = 1,
            REPAIR_REPLICA = 2,
            PROMOTE_REPLICA = 3,
            ISOLATE_NODE = 4,
            NOTIFY = 5
        };

        enum class HealingParamType : uint8_t
        {
            BOOL = 0,
            INT = 1,
            FLOAT = 2,
            STRING = 3,
            UUID = 4,
            JSON = 5
        };

        enum class HealingRunState : uint8_t
        {
            QUEUED = 0,
            RUNNING = 1,
            COMPLETED = 2,
            FAILED = 3,
            CANCELLED = 4
        };

        enum class HealingStepState : uint8_t
        {
            PENDING = 0,
            RUNNING = 1,
            COMPLETED = 2,
            FAILED = 3,
            SKIPPED = 4
        };

        struct WorkloadClassCatalogInfo
        {
            ID class_id;
            std::string class_name;
            std::string description;
            WorkloadMatchKind match_kind = WorkloadMatchKind::CUSTOM;
            ID match_expr_sblr_id;
            std::string match_text;
            bool has_default_role = false;
            ClusterNodeRole default_role = ClusterNodeRole::ROUTER;
            uint8_t priority = 0;
            bool has_max_latency_ms = false;
            uint32_t max_latency_ms = 0;
            bool allow_cross_shard = false;
            bool is_enabled = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct WorkloadRouteCatalogInfo
        {
            ID route_id;
            ID class_id;
            std::string route_name;
            RouteTargetKind target_kind = RouteTargetKind::NODE;
            ID target_uuid;
            std::string target_label;
            bool has_role = false;
            ClusterNodeRole role = ClusterNodeRole::ROUTER;
            bool has_service_type = false;
            ClusterServiceType service_type = ClusterServiceType::OLTP_RPC;
            ConnectionTransport transport = ConnectionTransport::LOCAL;
            uint16_t route_weight = 0;
            ID selector_expr_sblr_id;
            ID fallback_route_id;
            bool is_enabled = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct AdmissionPolicyCatalogInfo
        {
            ID policy_id;
            std::string policy_name;
            uint32_t max_concurrent_sessions = 0;
            uint32_t max_concurrent_queries = 0;
            uint32_t max_queue_depth = 0;
            uint8_t cpu_reject_pct = 0;
            uint8_t mem_reject_pct = 0;
            uint8_t io_reject_pct = 0;
            AdmissionRejectMode reject_mode = AdmissionRejectMode::REJECT;
            uint32_t queue_timeout_ms = 0;
            std::string accelerator_profile_name;
            uint64_t accelerator_memory_budget_bytes = 0;
            uint64_t accelerator_pinned_residency_target_bytes = 0;
            uint32_t accelerator_concurrent_build_limit = 0;
            uint32_t accelerator_concurrent_search_limit = 0;
            std::string accelerator_prewarm_policy;
            std::string accelerator_fallback_policy;
            std::string accelerator_degraded_state_override;
            bool is_enabled = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct AdmissionBindingCatalogInfo
        {
            ID binding_id;
            ID policy_id;
            AdmissionTargetKind target_kind = AdmissionTargetKind::CLUSTER;
            ID target_uuid;
            ID class_id;
            uint8_t priority = 0;
            std::string accelerator_device_class;
            std::string accelerator_device_id;
            std::string accelerator_device_pool_id;
            bool is_enabled = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct SloProfileCatalogInfo
        {
            ID slo_profile_id;
            std::string profile_name;
            ClusterNodeRole role = ClusterNodeRole::OLTP_DATA;
            double availability_target_pct = 0.0;
            uint32_t latency_p95_target_ms = 0;
            uint32_t latency_p99_target_ms = 0;
            double error_rate_target_pct = 0.0;
            uint32_t window_minutes = 0;
            uint32_t short_burn_window_minutes = 0;
            uint32_t long_burn_window_minutes = 0;
            double critical_burn_threshold = 0.0;
            double high_burn_threshold = 0.0;
            double moderate_burn_threshold = 0.0;
            uint64_t version_u64 = 0;
            bool is_active = true;
            bool is_valid = true;
        };

        struct SloBindingCatalogInfo
        {
            ID slo_binding_id;
            ID slo_profile_id;
            bool has_node_id = false;
            ID node_id;
            ClusterNodeRole role = ClusterNodeRole::OLTP_DATA;
            uint16_t priority_rank = 0;
            uint64_t effective_from_time = 0;
            bool has_effective_to_time = false;
            uint64_t effective_to_time = 0;
            uint64_t version_u64 = 0;
            bool is_valid = true;
        };

        struct SloWindowCatalogInfo
        {
            ID slo_window_id;
            ID node_id;
            ClusterNodeRole role = ClusterNodeRole::OLTP_DATA;
            uint64_t window_start_time = 0;
            uint64_t window_end_time = 0;
            uint64_t request_count = 0;
            uint64_t success_count = 0;
            uint64_t error_count = 0;
            uint32_t latency_p95_ms = 0;
            uint32_t latency_p99_ms = 0;
            double availability_sli_pct = 0.0;
            double error_rate_sli_pct = 0.0;
            uint64_t version_u64 = 0;
            bool is_valid = true;
        };

        struct SloBurnEventCatalogInfo
        {
            ID slo_burn_event_id;
            ID node_id;
            ClusterNodeRole role = ClusterNodeRole::OLTP_DATA;
            ID slo_profile_id;
            double short_burn_rate = 0.0;
            double long_burn_rate = 0.0;
            SloBurnSeverity burn_severity = SloBurnSeverity::NONE;
            SloActionPlan action_plan = SloActionPlan::NONE;
            uint64_t event_time = 0;
            bool has_resolved_time = false;
            uint64_t resolved_time = 0;
            bool is_valid = true;
        };

        struct AutoscalePolicyCatalogInfo
        {
            ID autoscale_policy_id;
            ClusterNodeRole role = ClusterNodeRole::OLTP_DATA;
            uint16_t min_nodes = 0;
            uint16_t max_nodes = 0;
            uint16_t scale_out_step = 0;
            uint16_t scale_in_step = 0;
            uint32_t scale_out_cooldown_ms = 0;
            uint32_t scale_in_cooldown_ms = 0;
            uint8_t cpu_scale_out_pct = 0;
            uint8_t queue_scale_out_pct = 0;
            double slo_burn_scale_out_threshold = 0.0;
            double slo_recovery_scale_in_threshold = 0.0;
            uint64_t version_u64 = 0;
            bool is_valid = true;
        };

        struct AutoscaleActionCatalogInfo
        {
            ID autoscale_action_id;
            ClusterNodeRole role = ClusterNodeRole::OLTP_DATA;
            AutoscaleActionKind action_kind = AutoscaleActionKind::NO_OP;
            int16_t requested_count_delta = 0;
            int16_t applied_count_delta = 0;
            std::string trigger_reason;
            double trigger_burn_rate = 0.0;
            uint64_t policy_version_u64 = 0;
            uint64_t action_time = 0;
            bool has_completed_time = false;
            uint64_t completed_time = 0;
            AutoscaleActionState action_state = AutoscaleActionState::PENDING;
            std::string failure_code;
            bool is_valid = true;
        };

        struct AdmissionTuningEventCatalogInfo
        {
            ID admission_tuning_event_id;
            ClusterNodeRole role = ClusterNodeRole::OLTP_DATA;
            uint32_t old_max_concurrent_queries = 0;
            uint32_t new_max_concurrent_queries = 0;
            uint32_t old_max_queue_depth = 0;
            uint32_t new_max_queue_depth = 0;
            uint32_t old_queue_timeout_ms = 0;
            uint32_t new_queue_timeout_ms = 0;
            std::string reason;
            uint64_t policy_version_u64 = 0;
            uint64_t event_time = 0;
            bool is_valid = true;
        };

        struct ClusterPolicyCatalogInfo
        {
            ID policy_id;
            ID cluster_id;
            std::string policy_name;
            ClusterPolicyKind policy_kind = ClusterPolicyKind::BASE;
            ID policy_json_uuid;
            bool has_policy_json_uuid = false;
            bool is_active = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct FailureDetectorCatalogInfo
        {
            ID detector_id;
            ID cluster_id;
            FailureDetectorKind detector_kind = FailureDetectorKind::PHI;
            uint32_t heartbeat_interval_ms = 0;
            bool has_phi_threshold = false;
            double phi_threshold = 0.0;
            bool has_miss_threshold = false;
            uint16_t miss_threshold = 0;
            bool has_suspect_threshold = false;
            uint16_t suspect_threshold = 0;
            bool has_fail_threshold = false;
            uint16_t fail_threshold = 0;
            uint32_t grace_startup_ms = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct AlertRuleCatalogInfo
        {
            ID rule_id;
            std::string rule_name;
            AlertRuleKind rule_kind = AlertRuleKind::METRIC;
            AlertSeverity severity = AlertSeverity::INFO;
            ID condition_sblr_uuid;
            bool has_condition_sblr_uuid = false;
            std::string condition_text;
            bool has_condition_text = false;
            uint32_t throttle_interval_ms = 0;
            bool is_enabled = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct AlertTargetCatalogInfo
        {
            ID target_id;
            std::string target_name;
            AlertTargetKind target_kind = AlertTargetKind::WEBHOOK;
            std::string endpoint;
            ID auth_secret_uuid;
            bool has_auth_secret_uuid = false;
            bool is_enabled = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct AlertRouteCatalogInfo
        {
            ID route_id;
            ID rule_id;
            ID target_id;
            AlertRouteKind route_kind = AlertRouteKind::IMMEDIATE;
            AlertSeverity severity_min = AlertSeverity::INFO;
            AlertSeverity severity_max = AlertSeverity::CRITICAL;
            bool is_enabled = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct AlertEventCatalogInfo
        {
            ID event_id;
            ID rule_id;
            AlertSeverity severity = AlertSeverity::INFO;
            AlertEventState event_state = AlertEventState::OPEN;
            uint64_t event_time = 0;
            bool has_resolved_time = false;
            uint64_t resolved_time = 0;
            bool has_event_payload_uuid = false;
            ID event_payload_uuid;
            bool is_valid = true;
        };

        struct AlertAckCatalogInfo
        {
            ID ack_id;
            ID event_id;
            ID user_id;
            uint64_t ack_time = 0;
            bool has_comment = false;
            std::string comment;
            bool is_valid = true;
        };

        struct AlertSilenceCatalogInfo
        {
            ID silence_id;
            AlertSilenceScope scope_kind = AlertSilenceScope::CLUSTER;
            bool has_scope_uuid = false;
            ID scope_uuid;
            uint64_t starts_time = 0;
            uint64_t ends_time = 0;
            ID created_by_uuid;
            bool has_reason = false;
            std::string reason;
            bool is_enabled = true;
            bool is_valid = true;
        };

        struct NetworkPartitionEventCatalogInfo
        {
            ID partition_id;
            ID cluster_id;
            PartitionState partition_state = PartitionState::OPEN;
            uint64_t opened_time = 0;
            bool has_resolved_time = false;
            uint64_t resolved_time = 0;
            bool quorum_reachable = false;
            ID local_node_id;
            bool has_description = false;
            std::string description;
            bool is_valid = true;
        };

        struct NetworkPartitionMemberCatalogInfo
        {
            ID member_id;
            ID partition_id;
            ID node_id;
            uint16_t side_id = 0;
            bool reachable = false;
            bool is_valid = true;
        };

        struct HealingPolicyCatalogInfo
        {
            ID policy_id;
            std::string policy_name;
            HealingTriggerKind trigger_kind = HealingTriggerKind::PARTITION;
            AlertSeverity min_severity = AlertSeverity::INFO;
            bool is_enabled = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct HealingActionCatalogInfo
        {
            ID action_id;
            ID policy_id;
            HealingActionKind action_kind = HealingActionKind::NOTIFY;
            uint16_t action_order = 0;
            bool is_blocking = false;
            uint16_t max_retries = 0;
            uint32_t cooldown_ms = 0;
            bool is_enabled = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct HealingActionParamCatalogInfo
        {
            ID param_id;
            ID action_id;
            std::string param_key;
            HealingParamType param_type = HealingParamType::STRING;
            bool has_val_u64 = false;
            uint64_t val_u64 = 0;
            bool has_val_i64 = false;
            int64_t val_i64 = 0;
            bool has_val_f64 = false;
            double val_f64 = 0.0;
            bool has_val_bool = false;
            bool val_bool = false;
            bool has_val_text = false;
            std::string val_text;
            bool has_val_uuid = false;
            ID val_uuid;
            bool has_val_json = false;
            std::string val_json;
            bool is_valid = true;
        };

        struct HealingRunCatalogInfo
        {
            ID run_id;
            ID policy_id;
            bool has_trigger_event_id = false;
            ID trigger_event_id;
            HealingRunState state = HealingRunState::QUEUED;
            uint64_t started_time = 0;
            bool has_completed_time = false;
            uint64_t completed_time = 0;
            bool has_error_message = false;
            std::string error_message;
            bool is_valid = true;
        };

        struct HealingStepCatalogInfo
        {
            ID step_id;
            ID run_id;
            ID action_id;
            uint16_t step_index = 0;
            HealingStepState state = HealingStepState::PENDING;
            bool has_started_time = false;
            uint64_t started_time = 0;
            bool has_completed_time = false;
            uint64_t completed_time = 0;
            bool has_error_message = false;
            std::string error_message;
            bool is_valid = true;
        };

        struct SecurityOperationsAutomationRequest
        {
            uint64_t now_time = 0;
            bool create_healing_runs = true;
            uint64_t info_ack_sla_ms = 86400000ULL;
            uint64_t warning_ack_sla_ms = 14400000ULL;
            uint64_t critical_ack_sla_ms = 900000ULL;
            uint64_t warning_vulnerability_sla_ms = 2592000000ULL;
            uint64_t critical_vulnerability_sla_ms = 604800000ULL;
        };

        struct SecurityOperationsAutomationAction
        {
            ID event_id;
            ID rule_id;
            std::string rule_name;
            AlertSeverity severity = AlertSeverity::INFO;
            bool vulnerability_signal = false;
            bool ack_overdue = false;
            bool remediation_overdue = false;
            uint64_t ack_deadline_time = 0;
            uint64_t remediation_deadline_time = 0;
            std::vector<ID> route_ids;
            std::vector<ID> target_ids;
            bool healing_run_created = false;
            ID healing_run_id;
            std::string action_code;
        };

        struct SecurityOperationsAutomationResult
        {
            uint64_t open_event_count = 0;
            uint64_t actionable_event_count = 0;
            uint64_t healing_run_count = 0;
            std::vector<SecurityOperationsAutomationAction> actions;
        };

        struct ClusterCatalogInfo
        {
            ID cluster_id;
            std::string cluster_name;
            ClusterMode cluster_mode = ClusterMode::WORKGROUP;
            ClusterState cluster_state = ClusterState::INIT;
            uint64_t cluster_state_version = 0;
            ConsensusMode consensus_mode = ConsensusMode::SINGLE;
            ID policy_id;
            uint64_t config_version = 0;
            uint64_t created_txid = 0;
            uint64_t last_modified_txid = 0;
            uint64_t created_time = 0;
            uint64_t last_state_change_time = 0;
            std::string description;
            bool is_valid = true;
        };

        struct ShardPolicyCatalogInfo
        {
            ID policy_id;
            std::string policy_name;
            uint16_t replication_factor = 0;
            ConsistencyLevel consistency_read = ConsistencyLevel::QUORUM;
            ConsistencyLevel consistency_write = ConsistencyLevel::QUORUM;
            FailoverMode failover_mode = FailoverMode::AUTO;
            RebalanceMode rebalance_mode = RebalanceMode::MANUAL;
            bool shard_key_required = false;
            bool allow_cross_shard_txn = false;
            uint32_t default_shard_count = 0;
            uint32_t shard_size_target_mb = 0;
            uint8_t shard_growth_trigger_pct = 0;
            uint64_t rebalance_interval_ms = 0;
            uint64_t created_txid = 0;
            uint64_t last_modified_txid = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct ShardPolicyParamCatalogInfo
        {
            ID policy_param_id;
            ID policy_id;
            std::string param_key;
            ShardPolicyParamType param_type = ShardPolicyParamType::U64;
            bool has_val_u64 = false;
            uint64_t val_u64 = 0;
            bool has_val_i64 = false;
            int64_t val_i64 = 0;
            bool has_val_f64 = false;
            double val_f64 = 0.0;
            bool has_val_bool = false;
            bool val_bool = false;
            bool has_val_text = false;
            std::string val_text;
            bool has_val_uuid = false;
            ID val_uuid;
            bool has_val_json = false;
            std::string val_json;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct ShardKeyCatalogInfo
        {
            ID shard_key_id;
            ID table_id;
            ShardKeyKind shard_key_kind = ShardKeyKind::HASH;
            ID key_columns_id;
            ID key_expression_sblr_id;
            HashFunctionKind hash_function = HashFunctionKind::MURMUR3;
            bool has_partition_count = false;
            uint32_t partition_count = 0;
            bool has_range_order = false;
            RangeOrderKind range_order = RangeOrderKind::ASC;
            uint32_t key_version = 1;
            bool is_active = true;
            uint64_t created_txid = 0;
            uint64_t last_modified_txid = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct ShardCatalogInfo
        {
            ID shard_id;
            std::string shard_name;
            ID cluster_id;
            ShardState shard_state = ShardState::CREATING;
            ShardKind shard_kind = ShardKind::ROW;
            ID policy_id;
            uint64_t created_txid = 0;
            uint64_t last_modified_txid = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct ShardScopeCatalogInfo
        {
            ID scope_id;
            ID shard_id;
            ID object_id;
            ObjectType object_kind = ObjectType::TABLE;
            ID shard_key_id;
            bool is_primary_scope = false;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct ShardRangeCatalogInfo
        {
            ID range_id;
            ID shard_id;
            ShardRangeKind range_kind = ShardRangeKind::TOKEN;
            ID range_type_id;
            std::string range_min_bytes;
            std::string range_max_bytes;
            bool has_range_min_s64 = false;
            int64_t range_min_s64 = 0;
            bool has_range_max_s64 = false;
            int64_t range_max_s64 = 0;
            bool inclusive_min = true;
            bool inclusive_max = false;
            bool has_hash_bucket = false;
            uint32_t hash_bucket = 0;
            std::string zone_tag;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct ShardReplicaCatalogInfo
        {
            ID replica_id;
            ID shard_id;
            ID node_id;
            ReplicaRole replica_role = ReplicaRole::SECONDARY;
            ReplicaState replica_state = ReplicaState::SYNCING;
            bool has_last_applied_txid = false;
            uint64_t last_applied_txid = 0;
            bool has_last_sync_time = false;
            uint64_t last_sync_time = 0;
            bool is_voting = false;
            uint8_t weight = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct ShardMigrationCatalogInfo
        {
            ID migration_id;
            ID shard_id;
            ID source_node_id;
            ID target_node_id;
            ShardMigrationState state = ShardMigrationState::PLANNED;
            uint64_t bytes_total = 0;
            uint64_t bytes_copied = 0;
            uint64_t rows_total = 0;
            uint64_t rows_copied = 0;
            ThrottleState throttle_state = ThrottleState::NONE;
            uint64_t started_time = 0;
            uint64_t updated_time = 0;
            bool has_completed_time = false;
            uint64_t completed_time = 0;
            std::string error_code;
            std::string error_message;
            bool is_valid = true;
        };

        struct ShardZoneCatalogInfo
        {
            ID zone_id;
            std::string zone_name;
            std::string description;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct ShardZoneRangeCatalogInfo
        {
            ID zone_range_id;
            ID zone_id;
            ID range_id;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        enum class ClockStateLabel : uint8_t
        {
            HEALTHY = 0,
            WARN = 1,
            SOFT_SKEW = 2,
            HARD_SKEW = 3,
            STALE = 4
        };

        enum class ClockActionTaken : uint8_t
        {
            NONE = 0,
            DEGRADE_WEIGHT = 1,
            READ_ONLY = 2,
            QUARANTINE = 3,
            FENCE_WRITES = 4
        };

        struct ClockPolicyCatalogInfo
        {
            ID clock_policy_id;
            std::string policy_name;
            uint32_t warn_skew_ms = 0;
            uint32_t soft_skew_ms = 0;
            uint32_t hard_skew_ms = 0;
            uint32_t max_jitter_ms = 0;
            uint32_t sample_interval_ms = 0;
            uint32_t stale_after_ms = 0;
            uint32_t skew_guard_ms = 0;
            bool node_quarantine_on_hard_skew = false;
            uint64_t version_u64 = 0;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
            bool is_valid = true;
        };

        struct ClockSourceCatalogInfo
        {
            ID clock_source_id;
            ID clock_policy_id;
            ClockSourceKind source_kind = ClockSourceKind::NTP;
            std::string endpoint;
            uint16_t priority_rank = 0;
            bool is_enabled = true;
            bool has_last_probe_time = false;
            uint64_t last_probe_time = 0;
            bool has_last_probe_offset_ms = false;
            int32_t last_probe_offset_ms = 0;
            bool has_last_probe_jitter_ms = false;
            uint32_t last_probe_jitter_ms = 0;
            uint64_t version_u64 = 0;
            bool is_valid = true;
        };

        struct NodeClockStateCatalogInfo
        {
            ID node_clock_state_id;
            ID node_id;
            ID clock_policy_id;
            ClockStateLabel clock_state = ClockStateLabel::HEALTHY;
            int32_t offset_ms = 0;
            uint32_t jitter_ms = 0;
            uint32_t sample_count = 0;
            uint64_t last_sync_time = 0;
            uint64_t last_transition_time = 0;
            uint32_t logical_counter = 0;
            uint64_t version_u64 = 0;
            bool is_valid = true;
        };

        struct ClockViolationEventCatalogInfo
        {
            ID clock_violation_event_id;
            ID node_id;
            ID clock_policy_id;
            ClockStateLabel clock_state = ClockStateLabel::WARN;
            int32_t offset_ms = 0;
            uint32_t jitter_ms = 0;
            ClockActionTaken action_taken = ClockActionTaken::NONE;
            uint64_t event_time = 0;
            bool has_resolved_time = false;
            uint64_t resolved_time = 0;
            uint64_t version_u64 = 0;
            bool is_valid = true;
        };

        struct EncryptionProfileCatalogInfo
        {
            ID profile_id;
            std::string profile_name;
            EncryptionAlgorithm cipher = EncryptionAlgorithm::AES_256_GCM;
            KdfAlgorithm kdf_algorithm = KdfAlgorithm::PBKDF2_SHA256;
            ID kdf_params_id;
            KeyRotationPolicy key_rotation_policy = KeyRotationPolicy::MANUAL;
            uint16_t min_shards_required = 1;
            uint32_t unlock_timeout_ms = 0;
            bool is_active = true;
            bool is_valid = true;
            uint64_t created_time = 0;
            uint64_t last_modified_time = 0;
        };

        struct EncryptionKeyCatalogInfo
        {
            ID key_id;
            ID profile_id;
            KeyMaterialKind key_kind = KeyMaterialKind::SYMMETRIC;
            EncryptionKeyStatus key_status = EncryptionKeyStatus::STAGED;
            ID key_material_encrypted_id;
            std::array<uint8_t, 32> key_material_hash{};
            uint32_t key_version = 0;
            uint64_t created_time = 0;
            uint64_t activated_time = 0;
            bool has_activated_time = false;
            uint64_t retired_time = 0;
            bool has_retired_time = false;
            bool is_valid = true;
        };

        struct EncryptionKeyShardCatalogInfo
        {
            ID shard_id;
            ID key_id;
            uint16_t shard_index = 0;
            uint16_t shard_total = 0;
            ID shard_material_encrypted_id;
            std::string holder_identity;
            uint64_t created_time = 0;
            uint64_t last_collected_time = 0;
            bool has_last_collected_time = false;
            bool is_valid = true;
        };

        struct EncryptionBootstrapInfoCatalogInfo
        {
            ID database_id;
            ID profile_id;
            ID active_key_id;
            uint16_t min_shards_required = 1;
            uint32_t unlock_timeout_ms = 0;
            std::string unlock_policy;
            uint64_t last_unlock_time = 0;
            bool has_last_unlock_time = false;
            UnlockResult last_unlock_result = UnlockResult::NOT_ATTEMPTED;
            uint64_t policy_version = 0;
            bool is_valid = true;
        };

        struct CryptoBaselineEvaluationRequest
        {
            CryptoProfileId crypto_profile_id = CryptoProfileId::MODERN_BASELINE;
            SecurityTierId security_tier = SecurityTierId::TIER_2_STANDARD;

            bool is_network_session = false;
            bool is_mtls = false;
            TlsVersion tls_version = TlsVersion::TLS_1_2;
            std::string tls_cipher_suite;

            CredentialKind credential_kind = CredentialKind::PASSWORD_ARGON2ID;
            bool is_privileged_account = false;
            bool require_password_rehash_check = false;
            uint32_t argon2_memory_kib = 0;
            uint32_t argon2_iterations = 0;
            uint32_t argon2_parallelism = 0;

            bool artifact_signed = true;
            std::string artifact_signature_algorithm;
            bool debug_unsigned_override = false;
            bool listener_enabled = true;

            EncryptionAlgorithm at_rest_algorithm = EncryptionAlgorithm::AES_256_GCM;
            uint16_t nonce_len_bytes = 12;
            bool aes_gcm_hw_available = true;
            bool chacha_fallback_explicit = false;

            KeyProviderKind primary_provider = KeyProviderKind::LOCAL_FILE_KEYSTORE;
            bool primary_provider_available = true;
            bool primary_provider_authorized = true;
            bool has_escrow_provider = false;
            KeyProviderKind escrow_provider = KeyProviderKind::EXTERNAL_KMS;
            bool escrow_provider_available = true;
            bool escrow_provider_authorized = true;

            bool evaluate_rotation_window = false;
            bool privileged_operation = false;
            bool has_encryption_profile_id = false;
            ID encryption_profile_id;
            bool has_active_key_id = false;
            ID active_key_id;
            uint64_t now_utc = 0;
        };

        struct CryptoBaselineEvaluationDecision
        {
            bool allowed = false;
            bool require_password_rehash = false;
            bool gate_pass = false;
            bool rotation_overdue = false;
            ID active_key_id;
            std::string reject_code;
        };

        struct EncryptionKeyLifecycleTransitionRequest
        {
            ID key_id;
            EncryptionKeyStatus target_status = EncryptionKeyStatus::STAGED;
            uint64_t event_time_utc = 0;
            bool retire_existing_active = false;
        };

        struct EncryptionKeyLifecycleTransitionDecision
        {
            bool applied = false;
            bool rotated_existing_active = false;
            ID retired_key_id;
            uint32_t resulting_key_version = 0;
            std::string reject_code;
        };

        struct ChannelSecurityEvaluationRequest
        {
            std::string channel_name;
            CertKind cert_kind = CertKind::SERVER;
            bool is_tls = false;
            bool is_mtls = false;
            TlsVersion tls_version = TlsVersion::TLS_1_2;
            std::string tls_cipher_suite;
            bool has_presented_cert_id = false;
            ID presented_cert_id;
            CryptoProfileId crypto_profile_id = CryptoProfileId::MODERN_BASELINE;
            SecurityTierId security_tier = SecurityTierId::TIER_2_STANDARD;
            uint64_t distribution_stale_after_ms = 10000;
            uint64_t now_time = 0;
        };

        struct ChannelSecurityEvaluationDecision
        {
            bool allowed = false;
            bool binding_found = false;
            bool cert_active = false;
            bool trust_anchor_active = false;
            bool revocation_stale = false;
            bool rotation_required = false;
            ID binding_id;
            ID cert_id;
            std::string reject_code;
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
            ID mcv_oid{};         // TOAST reference for MCV list (JSON)
            ID histogram_oid{};   // TOAST reference for histogram (JSON)

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
        auto purgeStaleSessionTemporaryTables(ErrorContext *ctx = nullptr) -> Status;

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

        /**
         * Persist an updated root GPID for an existing index.
         *
         * Used by runtime index structures when splits or rebuild paths move the
         * authoritative root page and fresh opens must observe the new root.
         */
        auto updateIndexRootGPID(const ID &index_id, GPID root_gpid,
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
         * Reopen a cached index object from authoritative catalog metadata.
         * Used when low-level repair or rollback paths need the runtime handle
         * to reflect on-disk state changes immediately.
         */
        auto refreshIndexObject(const ID &index_id, ErrorContext *ctx = nullptr) -> Status;

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
                        const TempObjectOptions* temp_opts = nullptr,
                        bool with_data = true) -> Status;
        auto setViewExecutionMetadata(const ID& view_id,
                                      const std::vector<uint8_t>& compiled_query_sblr,
                                      const std::vector<ViewColumnBinding>& insert_bindings,
                                      const std::string& source_dialect = std::string(),
                                      const std::vector<uint8_t>& native_compiled_code = {},
                                      ErrorContext* ctx = nullptr) -> Status;

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

        // ============================================================================
        // Canonical type catalog operations (CAT-010)
        // ============================================================================

        auto upsertTypeCatalogEntry(const TypeCatalogInfo& info,
                                    ID& type_id_out,
                                    ErrorContext* ctx = nullptr) -> Status;
        auto getTypeCatalogEntry(const ID& type_id,
                                 TypeCatalogInfo& info_out,
                                 ErrorContext* ctx = nullptr) -> Status;
        auto getTypeCatalogEntryByName(const ID& schema_id,
                                       const std::string& type_name,
                                       TypeCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listTypeCatalogEntries(const ID& schema_id,
                                    std::vector<TypeCatalogInfo>& types_out,
                                    ErrorContext* ctx = nullptr) -> Status;
        auto deleteTypeCatalogEntry(const ID& type_id,
                                    ErrorContext* ctx = nullptr) -> Status;

        auto upsertTypeModifier(const TypeModifierInfo& info,
                                ErrorContext* ctx = nullptr) -> Status;
        auto getTypeModifier(const ID& type_id,
                             uint16_t modifier_key,
                             TypeModifierInfo& info_out,
                             ErrorContext* ctx = nullptr) -> Status;
        auto listTypeModifiers(const ID& type_id,
                               std::vector<TypeModifierInfo>& modifiers_out,
                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteTypeModifier(const ID& type_id,
                                uint16_t modifier_key,
                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertTypeIo(const TypeIoInfo& info,
                          ErrorContext* ctx = nullptr) -> Status;
        auto getTypeIo(const ID& type_id,
                       TypeIoInfo& info_out,
                       ErrorContext* ctx = nullptr) -> Status;
        auto listTypeIo(std::vector<TypeIoInfo>& rows_out,
                        ErrorContext* ctx = nullptr) -> Status;
        auto deleteTypeIo(const ID& type_id,
                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertTypeCast(const TypeCastInfo& info,
                            ErrorContext* ctx = nullptr) -> Status;
        auto getTypeCast(const ID& source_type_id,
                         const ID& target_type_id,
                         TypeCastKind cast_kind,
                         TypeCastInfo& info_out,
                         ErrorContext* ctx = nullptr) -> Status;
        auto listTypeCasts(std::vector<TypeCastInfo>& casts_out,
                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteTypeCast(const ID& source_type_id,
                            const ID& target_type_id,
                            TypeCastKind cast_kind,
                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertTypeTransform(const TypeTransformInfo& info,
                                 ID& transform_id_out,
                                 ErrorContext* ctx = nullptr) -> Status;
        auto getTypeTransform(const ID& transform_id,
                              TypeTransformInfo& info_out,
                              ErrorContext* ctx = nullptr) -> Status;
        auto getTypeTransformByTypeLanguage(const ID& type_id,
                                            const ID& language_id,
                                            TypeTransformInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listTypeTransforms(const ID& type_id,
                                std::vector<TypeTransformInfo>& rows_out,
                                ErrorContext* ctx = nullptr) -> Status;
        auto deleteTypeTransform(const ID& transform_id,
                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertEncodingConversion(const EncodingConversionInfo& info,
                                      ID& conversion_id_out,
                                      ErrorContext* ctx = nullptr) -> Status;
        auto getEncodingConversion(const ID& conversion_id,
                                   EncodingConversionInfo& info_out,
                                   ErrorContext* ctx = nullptr) -> Status;
        auto getEncodingConversionByName(const std::string& conversion_name,
                                         EncodingConversionInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listEncodingConversions(std::vector<EncodingConversionInfo>& rows_out,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto deleteEncodingConversion(const ID& conversion_id,
                                      ErrorContext* ctx = nullptr) -> Status;

        // ============================================================================
        // Canonical domain extension catalog operations (CAT-011)
        // ============================================================================

        auto upsertDomainParamKey(const DomainParamKeyCatalogInfo& info,
                                  ErrorContext* ctx = nullptr) -> Status;
        auto getDomainParamKey(uint16_t param_key_id,
                               DomainParamKeyCatalogInfo& info_out,
                               ErrorContext* ctx = nullptr) -> Status;
        auto getDomainParamKeyByName(const std::string& param_name,
                                     DomainParamKeyCatalogInfo& info_out,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto listDomainParamKeys(std::vector<DomainParamKeyCatalogInfo>& rows_out,
                                 ErrorContext* ctx = nullptr) -> Status;
        auto deleteDomainParamKey(uint16_t param_key_id,
                                  ErrorContext* ctx = nullptr) -> Status;

        auto upsertDomainParameter(const DomainParameterCatalogInfo& info,
                                   ErrorContext* ctx = nullptr) -> Status;
        auto getDomainParameter(const ID& domain_id,
                                uint16_t param_key_id,
                                DomainParameterCatalogInfo& info_out,
                                ErrorContext* ctx = nullptr) -> Status;
        auto listDomainParameters(const ID& domain_id,
                                  std::vector<DomainParameterCatalogInfo>& rows_out,
                                  ErrorContext* ctx = nullptr) -> Status;
        auto deleteDomainParameter(const ID& domain_id,
                                   uint16_t param_key_id,
                                   ErrorContext* ctx = nullptr) -> Status;

        auto upsertDomainConstraintCatalogEntry(const DomainConstraintCatalogInfo& info,
                                                ID& constraint_id_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getDomainConstraintCatalogEntry(const ID& constraint_id,
                                             DomainConstraintCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listDomainConstraintCatalogEntries(const ID& domain_id,
                                                std::vector<DomainConstraintCatalogInfo>& rows_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto deleteDomainConstraintCatalogEntry(const ID& constraint_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertDomainSecurityCatalogEntry(const DomainSecurityCatalogInfo& info,
                                              ID& security_id_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getDomainSecurityCatalogEntry(const ID& security_id,
                                           DomainSecurityCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listDomainSecurityCatalogEntries(const ID& domain_id,
                                              std::vector<DomainSecurityCatalogInfo>& rows_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto deleteDomainSecurityCatalogEntry(const ID& security_id,
                                              ErrorContext* ctx = nullptr) -> Status;

        auto upsertDomainValidationCatalogEntry(const DomainValidationCatalogInfo& info,
                                                ID& validation_id_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getDomainValidationCatalogEntry(const ID& validation_id,
                                             DomainValidationCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listDomainValidationCatalogEntries(const ID& domain_id,
                                                std::vector<DomainValidationCatalogInfo>& rows_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto deleteDomainValidationCatalogEntry(const ID& validation_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertDomainIntegrityCatalogEntry(const DomainIntegrityCatalogInfo& info,
                                               ID& integrity_id_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getDomainIntegrityCatalogEntry(const ID& integrity_id,
                                            DomainIntegrityCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listDomainIntegrityCatalogEntries(const ID& domain_id,
                                               std::vector<DomainIntegrityCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteDomainIntegrityCatalogEntry(const ID& integrity_id,
                                               ErrorContext* ctx = nullptr) -> Status;

        // ============================================================================
        // Canonical charset/collation extension catalog operations (CAT-012)
        // ============================================================================

        auto upsertCharsetAliasCatalogEntry(const CharsetAliasCatalogInfo& info,
                                            ID& alias_id_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getCharsetAliasCatalogEntry(const ID& alias_id,
                                         CharsetAliasCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto getCharsetAliasCatalogEntryByNormalizedName(const std::string& normalized_name,
                                                         CharsetAliasCatalogInfo& info_out,
                                                         ErrorContext* ctx = nullptr) -> Status;
        auto listCharsetAliasCatalogEntries(const ID& charset_id,
                                            std::vector<CharsetAliasCatalogInfo>& rows_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto deleteCharsetAliasCatalogEntry(const ID& alias_id,
                                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertCollationTailoringCatalogEntry(const CollationTailoringCatalogInfo& info,
                                                  ID& tailoring_id_out,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto getCollationTailoringCatalogEntry(const ID& tailoring_id,
                                               CollationTailoringCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listCollationTailoringCatalogEntries(
            uint32_t collation_id,
            std::vector<CollationTailoringCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteCollationTailoringCatalogEntry(const ID& tailoring_id,
                                                  ErrorContext* ctx = nullptr) -> Status;

        // ============================================================================
        // Canonical resource/timezone extension catalog operations (CAT-013)
        // ============================================================================

        auto upsertResourceBundleCatalogEntry(const ResourceBundleCatalogInfo& info,
                                              ID& bundle_id_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getResourceBundleCatalogEntry(const ID& bundle_id,
                                           ResourceBundleCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getActiveResourceBundleByKind(ResourceBundleKind kind,
                                           ResourceBundleCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listResourceBundleCatalogEntries(ResourceBundleKind kind_filter,
                                              std::vector<ResourceBundleCatalogInfo>& rows_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto deleteResourceBundleCatalogEntry(const ID& bundle_id,
                                              ErrorContext* ctx = nullptr) -> Status;

        auto upsertResourceArtifactCatalogEntry(const ResourceArtifactCatalogInfo& info,
                                                ID& artifact_id_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getResourceArtifactCatalogEntry(const ID& artifact_id,
                                             ResourceArtifactCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listResourceArtifactCatalogEntries(const ID& bundle_id,
                                                std::vector<ResourceArtifactCatalogInfo>& rows_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto deleteResourceArtifactCatalogEntry(const ID& artifact_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertTimezoneTransitionCatalogEntry(const TimezoneTransitionCatalogInfo& info,
                                                  ID& transition_id_out,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto getTimezoneTransitionCatalogEntry(const ID& transition_id,
                                               TimezoneTransitionCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listTimezoneTransitionCatalogEntries(const ID& timezone_id,
                                                  std::vector<TimezoneTransitionCatalogInfo>& rows_out,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto deleteTimezoneTransitionCatalogEntry(const ID& transition_id,
                                                  ErrorContext* ctx = nullptr) -> Status;

        auto upsertTimezoneLeapSecondCatalogEntry(const TimezoneLeapSecondCatalogInfo& info,
                                                  ID& leap_id_out,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto getTimezoneLeapSecondCatalogEntry(const ID& leap_id,
                                               TimezoneLeapSecondCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listTimezoneLeapSecondCatalogEntries(const ID& bundle_id,
                                                  std::vector<TimezoneLeapSecondCatalogInfo>& rows_out,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto deleteTimezoneLeapSecondCatalogEntry(const ID& leap_id,
                                                  ErrorContext* ctx = nullptr) -> Status;

        // ============================================================================
        // Canonical reserved-word/parser capability catalog operations (CAT-014)
        // ============================================================================

        auto upsertReservedWordCatalogEntry(const ReservedWordCatalogInfo& info,
                                            ID& reserved_word_id_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getReservedWordCatalogEntry(const ID& reserved_word_id,
                                         ReservedWordCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto getReservedWordCatalogEntryByWord(const std::string& word,
                                               EmulationEngine parser_scope,
                                               ReservedWordCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listReservedWordCatalogEntries(
            EmulationEngine parser_scope_filter,
            std::vector<ReservedWordCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteReservedWordCatalogEntry(const ID& reserved_word_id,
                                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertEmulationProfileCatalogEntry(const EmulationProfileCatalogInfo& info,
                                                ID& emulation_profile_id_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getEmulationProfileCatalogEntry(const ID& emulation_profile_id,
                                             EmulationProfileCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getEmulationProfileCatalogEntryByEngine(EmulationEngine engine,
                                                     EmulationProfileCatalogInfo& info_out,
                                                     ErrorContext* ctx = nullptr) -> Status;
        auto listEmulationProfileCatalogEntries(
            std::vector<EmulationProfileCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteEmulationProfileCatalogEntry(const ID& emulation_profile_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertParserProfileCatalogEntry(const ParserProfileCatalogInfo& info,
                                             ID& parser_profile_id_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getParserProfileCatalogEntry(const ID& parser_profile_id,
                                          ParserProfileCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getParserProfileCatalogEntryByName(const std::string& profile_name,
                                                ParserProfileCatalogInfo& info_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto listParserProfileCatalogEntries(
            EmulationEngine parser_engine_filter,
            std::vector<ParserProfileCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteParserProfileCatalogEntry(const ID& parser_profile_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertParserTransformCatalogEntry(const ParserTransformCatalogInfo& info,
                                               ID& parser_transform_id_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getParserTransformCatalogEntry(const ID& parser_transform_id,
                                            ParserTransformCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listParserTransformCatalogEntries(
            const ID& parser_profile_id,
            std::vector<ParserTransformCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteParserTransformCatalogEntry(const ID& parser_transform_id,
                                               ErrorContext* ctx = nullptr) -> Status;

        auto upsertParserCapabilityCatalogEntry(const ParserCapabilityCatalogInfo& info,
                                                ID& parser_capability_id_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getParserCapabilityCatalogEntry(const ID& parser_capability_id,
                                             ParserCapabilityCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listParserCapabilityCatalogEntries(
            const ID& parser_profile_id,
            const std::string& feature_family,
            std::vector<ParserCapabilityCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteParserCapabilityCatalogEntry(const ID& parser_capability_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertParserErrorMapCatalogEntry(const ParserErrorMapCatalogInfo& info,
                                              ID& parser_error_map_id_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getParserErrorMapCatalogEntry(const ID& parser_error_map_id,
                                           ParserErrorMapCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getParserErrorMapCatalogEntryByRejectCode(const ID& parser_profile_id,
                                                       const std::string& reject_code,
                                                       ParserErrorMapCatalogInfo& info_out,
                                                       ErrorContext* ctx = nullptr) -> Status;
        auto listParserErrorMapCatalogEntries(
            const ID& parser_profile_id,
            std::vector<ParserErrorMapCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteParserErrorMapCatalogEntry(const ID& parser_error_map_id,
                                              ErrorContext* ctx = nullptr) -> Status;

        auto upsertParserFeaturePrecedenceCatalogEntry(
            const ParserFeaturePrecedenceCatalogInfo& info,
            ID& parser_feature_precedence_id_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto getParserFeaturePrecedenceCatalogEntry(
            const ID& parser_feature_precedence_id,
            ParserFeaturePrecedenceCatalogInfo& info_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto listParserFeaturePrecedenceCatalogEntries(
            const ID& parser_profile_id,
            const std::string& feature_family,
            std::vector<ParserFeaturePrecedenceCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteParserFeaturePrecedenceCatalogEntry(
            const ID& parser_feature_precedence_id,
            ErrorContext* ctx = nullptr) -> Status;

        // ============================================================================
        // Canonical relation extension catalog operations (CAT-015)
        // ============================================================================

        auto upsertPartitionedTableCatalogEntry(const PartitionedTableCatalogInfo& info,
                                                ID& partitioned_table_id_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getPartitionedTableCatalogEntry(const ID& partitioned_table_id,
                                             PartitionedTableCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getPartitionedTableCatalogEntryByTable(const ID& table_id,
                                                    PartitionedTableCatalogInfo& info_out,
                                                    ErrorContext* ctx = nullptr) -> Status;
        auto listPartitionedTableCatalogEntries(
            std::vector<PartitionedTableCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deletePartitionedTableCatalogEntry(const ID& partitioned_table_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertPartitionCatalogEntry(const PartitionCatalogInfo& info,
                                         ID& partition_id_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto getPartitionCatalogEntry(const ID& partition_id,
                                      PartitionCatalogInfo& info_out,
                                      ErrorContext* ctx = nullptr) -> Status;
        auto listPartitionCatalogEntries(const ID& parent_table_id,
                                         std::vector<PartitionCatalogInfo>& rows_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto deletePartitionCatalogEntry(const ID& partition_id,
                                         ErrorContext* ctx = nullptr) -> Status;

        auto upsertTableInheritanceCatalogEntry(const TableInheritanceCatalogInfo& info,
                                                ID& inheritance_id_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getTableInheritanceCatalogEntry(const ID& inheritance_id,
                                             TableInheritanceCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listTableInheritanceCatalogEntries(const ID& parent_table_id,
                                                std::vector<TableInheritanceCatalogInfo>& rows_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto deleteTableInheritanceCatalogEntry(const ID& inheritance_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertLanguageCatalogEntry(const LanguageCatalogInfo& info,
                                        ID& language_id_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto getLanguageCatalogEntry(const ID& language_id,
                                     LanguageCatalogInfo& info_out,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto getLanguageCatalogEntryByName(const std::string& language_name,
                                           LanguageCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listLanguageCatalogEntries(std::vector<LanguageCatalogInfo>& rows_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto deleteLanguageCatalogEntry(const ID& language_id,
                                        ErrorContext* ctx = nullptr) -> Status;

        auto upsertEventCatalogEntry(const EventCatalogInfo& info,
                                     ID& event_id_out,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto getEventCatalogEntry(const ID& event_id,
                                  EventCatalogInfo& info_out,
                                  ErrorContext* ctx = nullptr) -> Status;
        auto listEventCatalogEntries(const ID& schema_id,
                                     std::vector<EventCatalogInfo>& rows_out,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto deleteEventCatalogEntry(const ID& event_id,
                                     ErrorContext* ctx = nullptr) -> Status;

        auto upsertPackageMemberCatalogEntry(const PackageMemberCatalogInfo& info,
                                             ID& member_id_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getPackageMemberCatalogEntry(const ID& member_id,
                                          PackageMemberCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listPackageMemberCatalogEntries(const ID& package_id,
                                             std::vector<PackageMemberCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deletePackageMemberCatalogEntry(const ID& member_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        // ============================================================================
        // Canonical index metadata extension catalog operations (CAT-016)
        // ============================================================================

        auto upsertIndexAccessMethodCatalogEntry(const IndexAccessMethodCatalogInfo& info,
                                                 ID& access_method_id_out,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getIndexAccessMethodCatalogEntry(const ID& access_method_id,
                                              IndexAccessMethodCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getIndexAccessMethodCatalogEntryByName(const std::string& method_name,
                                                    IndexAccessMethodCatalogInfo& info_out,
                                                    ErrorContext* ctx = nullptr) -> Status;
        auto listIndexAccessMethodCatalogEntries(
            std::vector<IndexAccessMethodCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteIndexAccessMethodCatalogEntry(const ID& access_method_id,
                                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertIndexOpclassCatalogEntry(const IndexOpclassCatalogInfo& info,
                                            ID& opclass_id_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getIndexOpclassCatalogEntry(const ID& opclass_id,
                                         IndexOpclassCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listIndexOpclassCatalogEntries(const std::string& index_type_name,
                                            std::vector<IndexOpclassCatalogInfo>& rows_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto deleteIndexOpclassCatalogEntry(const ID& opclass_id,
                                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertIndexOpclassFunctionCatalogEntry(const IndexOpclassFunctionCatalogInfo& info,
                                                    ID& opclass_fn_id_out,
                                                    ErrorContext* ctx = nullptr) -> Status;
        auto getIndexOpclassFunctionCatalogEntry(const ID& opclass_fn_id,
                                                 IndexOpclassFunctionCatalogInfo& info_out,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto listIndexOpclassFunctionCatalogEntries(
            const ID& opclass_id,
            std::vector<IndexOpclassFunctionCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteIndexOpclassFunctionCatalogEntry(const ID& opclass_fn_id,
                                                    ErrorContext* ctx = nullptr) -> Status;

        auto upsertIndexColumnCatalogEntry(const IndexColumnCatalogInfo& info,
                                           ID& index_column_id_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getIndexColumnCatalogEntry(const ID& index_column_id,
                                        IndexColumnCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listIndexColumnCatalogEntries(const ID& index_id,
                                           std::vector<IndexColumnCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteIndexColumnCatalogEntry(const ID& index_column_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertIndexOptionCatalogEntry(const IndexOptionCatalogInfo& info,
                                           ID& option_id_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getIndexOptionCatalogEntry(const ID& option_id,
                                        IndexOptionCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listIndexOptionCatalogEntries(const ID& index_id,
                                           std::vector<IndexOptionCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteIndexOptionCatalogEntry(const ID& option_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertIndexMaintenanceCatalogEntry(const IndexMaintenanceCatalogInfo& info,
                                                ID& maintenance_id_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getIndexMaintenanceCatalogEntry(const ID& maintenance_id,
                                             IndexMaintenanceCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listIndexMaintenanceCatalogEntries(const ID& index_id,
                                                std::vector<IndexMaintenanceCatalogInfo>& rows_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto deleteIndexMaintenanceCatalogEntry(const ID& maintenance_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertIndexMaintenanceDeltaCatalogEntry(
            const IndexMaintenanceDeltaCatalogInfo& info,
            ID& maintenance_delta_id_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto getIndexMaintenanceDeltaCatalogEntry(const ID& maintenance_delta_id,
                                                  IndexMaintenanceDeltaCatalogInfo& info_out,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto listIndexMaintenanceDeltaCatalogEntries(
            const ID& maintenance_id,
            std::vector<IndexMaintenanceDeltaCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteIndexMaintenanceDeltaCatalogEntry(const ID& maintenance_delta_id,
                                                     ErrorContext* ctx = nullptr) -> Status;

        auto upsertIndexBuildDeltaCatalogEntry(const IndexBuildDeltaCatalogInfo& info,
                                               ID& build_delta_id_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getIndexBuildDeltaCatalogEntry(const ID& build_delta_id,
                                            IndexBuildDeltaCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listIndexBuildDeltaCatalogEntries(const ID& index_id,
                                               std::vector<IndexBuildDeltaCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteIndexBuildDeltaCatalogEntry(const ID& build_delta_id,
                                               ErrorContext* ctx = nullptr) -> Status;

        auto upsertIndexPageDeltaCatalogEntry(const IndexPageDeltaCatalogInfo& info,
                                              ID& page_delta_id_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getIndexPageDeltaCatalogEntry(const ID& page_delta_id,
                                           IndexPageDeltaCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listIndexPageDeltaCatalogEntries(const ID& index_id,
                                              std::vector<IndexPageDeltaCatalogInfo>& rows_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto deleteIndexPageDeltaCatalogEntry(const ID& page_delta_id,
                                              ErrorContext* ctx = nullptr) -> Status;

        // ============================================================================
        // Canonical index telemetry extension catalog operations (CAT-017)
        // ============================================================================

        auto upsertIndexStatsCatalogEntry(const IndexStatsCatalogInfo& info,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getIndexStatsCatalogEntry(const ID& index_id,
                                       IndexStatsCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listIndexStatsCatalogEntries(std::vector<IndexStatsCatalogInfo>& rows_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteIndexStatsCatalogEntry(const ID& index_id,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertIndexUsageCatalogEntry(const IndexUsageCatalogInfo& info,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getIndexUsageCatalogEntry(const ID& index_id,
                                       IndexUsageCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listIndexUsageCatalogEntries(std::vector<IndexUsageCatalogInfo>& rows_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteIndexUsageCatalogEntry(const ID& index_id,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertIndexContentionCatalogEntry(const IndexContentionCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getIndexContentionCatalogEntry(const ID& index_id,
                                            IndexContentionCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listIndexContentionCatalogEntries(std::vector<IndexContentionCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteIndexContentionCatalogEntry(const ID& index_id,
                                               ErrorContext* ctx = nullptr) -> Status;

        auto upsertIndexStorageCatalogEntry(const IndexStorageCatalogInfo& info,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getIndexStorageCatalogEntry(const ID& index_id,
                                         IndexStorageCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listIndexStorageCatalogEntries(std::vector<IndexStorageCatalogInfo>& rows_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto deleteIndexStorageCatalogEntry(const ID& index_id,
                                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertIndexHealthCatalogEntry(const IndexHealthCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getIndexHealthCatalogEntry(const ID& index_id,
                                        IndexHealthCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listIndexHealthCatalogEntries(std::vector<IndexHealthCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteIndexHealthCatalogEntry(const ID& index_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        // ============================================================================
        // Canonical storage extension catalog operations (CAT-018)
        // ============================================================================

        auto upsertFilespaceStatsCatalogEntry(const FilespaceStatsCatalogInfo& info,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getFilespaceStatsCatalogEntry(uint32_t filespace_id,
                                           FilespaceStatsCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listFilespaceStatsCatalogEntries(std::vector<FilespaceStatsCatalogInfo>& rows_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto deleteFilespaceStatsCatalogEntry(uint32_t filespace_id,
                                              ErrorContext* ctx = nullptr) -> Status;

        auto upsertLobCatalogEntry(const LobCatalogInfo& info,
                                   ErrorContext* ctx = nullptr) -> Status;
        auto getLobCatalogEntry(const ID& lob_id,
                                LobCatalogInfo& info_out,
                                ErrorContext* ctx = nullptr) -> Status;
        auto listLobCatalogEntries(std::vector<LobCatalogInfo>& rows_out,
                                   ErrorContext* ctx = nullptr) -> Status;
        auto deleteLobCatalogEntry(const ID& lob_id,
                                   ErrorContext* ctx = nullptr) -> Status;

        auto upsertLobPageCatalogEntry(const LobPageCatalogInfo& info,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto getLobPageCatalogEntry(const ID& lob_page_id,
                                    LobPageCatalogInfo& info_out,
                                    ErrorContext* ctx = nullptr) -> Status;
        auto listLobPageCatalogEntries(const ID& lob_id,
                                       std::vector<LobPageCatalogInfo>& rows_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto deleteLobPageCatalogEntry(const ID& lob_page_id,
                                       ErrorContext* ctx = nullptr) -> Status;

        auto upsertBackupHistoryCatalogEntry(const BackupHistoryCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getBackupHistoryCatalogEntry(const ID& backup_id,
                                          BackupHistoryCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listBackupHistoryCatalogEntries(const ID& database_id,
                                             std::vector<BackupHistoryCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteBackupHistoryCatalogEntry(const ID& backup_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertAuditSinkProfileCatalogEntry(const AuditSinkProfileCatalogInfo& info,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getAuditSinkProfileCatalogEntry(const ID& audit_sink_profile_id,
                                             AuditSinkProfileCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listAuditSinkProfileCatalogEntries(std::vector<AuditSinkProfileCatalogInfo>& rows_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto deleteAuditSinkProfileCatalogEntry(const ID& audit_sink_profile_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto appendAuditExportSegmentCatalogEntry(const AuditExportSegmentCatalogInfo& info,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto getAuditExportSegmentCatalogEntry(const ID& audit_export_segment_id,
                                               AuditExportSegmentCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listAuditExportSegmentCatalogEntries(const ID& audit_sink_profile_id,
                                                  std::vector<AuditExportSegmentCatalogInfo>& rows_out,
                                                  ErrorContext* ctx = nullptr) -> Status;

        auto appendTransactionLineageEventCatalogEntry(TransactionLineageEventCatalogInfo& info,
                                                       ErrorContext* ctx = nullptr) -> Status;
        auto getTransactionLineageEventCatalogEntry(const ID& lineage_event_id,
                                                    TransactionLineageEventCatalogInfo& info_out,
                                                    ErrorContext* ctx = nullptr) -> Status;
        auto listTransactionLineageEventCatalogEntries(
            const ID& tx_uuid,
            uint64_t txid,
            std::vector<TransactionLineageEventCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;

        auto appendSchemaEpochCatalogEntry(SchemaEpochCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getSchemaEpochCatalogEntry(const ID& schema_epoch_uuid,
                                        SchemaEpochCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listSchemaEpochCatalogEntries(const ID& database_id,
                                           std::vector<SchemaEpochCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getLatestSchemaEpochCatalogEntry(const ID& database_id,
                                              SchemaEpochCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto buildCurrentSchemaEpochDefinitionManifest(std::string& manifest_out,
                                                       ErrorContext* ctx = nullptr) -> Status;
        auto appendSchemaChangePlanCatalogEntry(SchemaChangePlanCatalogInfo& info,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getSchemaChangePlanCatalogEntry(const ID& schema_change_plan_uuid,
                                             SchemaChangePlanCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listSchemaChangePlanCatalogEntries(
            std::vector<SchemaChangePlanCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto appendSchemaChangeEventCatalogEntry(SchemaChangeEventCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto listSchemaChangeEventCatalogEntries(
            const ID& schema_change_plan_uuid,
            std::vector<SchemaChangeEventCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto upsertSchemaChangeBackfillProgressCatalogEntry(
            const SchemaChangeBackfillProgressCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getSchemaChangeBackfillProgressCatalogEntry(
            const ID& schema_change_plan_uuid,
            SchemaChangeBackfillProgressCatalogInfo& info_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto upsertSchemaChangeCutoverGuardCatalogEntry(
            const SchemaChangeCutoverGuardCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getSchemaChangeCutoverGuardCatalogEntry(
            const ID& schema_change_plan_uuid,
            SchemaChangeCutoverGuardCatalogInfo& info_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto appendIndexBuildPlanCatalogEntry(IndexBuildPlanCatalogInfo& info,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto updateIndexBuildPlanCatalogState(const ID& index_build_plan_uuid,
                                              const std::string& build_state,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getIndexBuildPlanCatalogEntry(const ID& index_build_plan_uuid,
                                           IndexBuildPlanCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listIndexBuildPlanCatalogEntries(
            std::vector<IndexBuildPlanCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto appendIndexBuildEventCatalogEntry(IndexBuildEventCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listIndexBuildEventCatalogEntries(
            const ID& index_build_plan_uuid,
            std::vector<IndexBuildEventCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto upsertIndexBuildProgressCatalogEntry(
            const IndexBuildProgressCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getIndexBuildProgressCatalogEntry(
            const ID& index_build_plan_uuid,
            IndexBuildProgressCatalogInfo& info_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto upsertIndexBuildCutoverGuardCatalogEntry(
            const IndexBuildCutoverGuardCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getIndexBuildCutoverGuardCatalogEntry(
            const ID& index_build_plan_uuid,
            IndexBuildCutoverGuardCatalogInfo& info_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto appendBulkLoadPlanCatalogEntry(BulkLoadPlanCatalogInfo& info,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto updateBulkLoadPlanCatalogPhaseState(const ID& bulk_load_plan_uuid,
                                                 const std::string& phase_state,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getBulkLoadPlanCatalogEntry(const ID& bulk_load_plan_uuid,
                                         BulkLoadPlanCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listBulkLoadPlanCatalogEntries(
            std::vector<BulkLoadPlanCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto appendBulkLoadEventCatalogEntry(BulkLoadEventCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listBulkLoadEventCatalogEntries(
            const ID& bulk_load_plan_uuid,
            std::vector<BulkLoadEventCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto upsertBulkLoadProgressCatalogEntry(
            const BulkLoadProgressCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getBulkLoadProgressCatalogEntry(
            const ID& bulk_load_plan_uuid,
            BulkLoadProgressCatalogInfo& info_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto upsertBulkLoadCutoverGuardCatalogEntry(
            const BulkLoadCutoverGuardCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getBulkLoadCutoverGuardCatalogEntry(
            const ID& bulk_load_plan_uuid,
            BulkLoadCutoverGuardCatalogInfo& info_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto upsertMemoryGrantFeedbackCatalogEntry(
            const MemoryGrantFeedbackCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getMemoryGrantFeedbackCatalogEntry(
            uint64_t grant_key_hash,
            MemoryGrantFeedbackCatalogInfo& info_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto listMemoryGrantFeedbackCatalogEntries(
            std::vector<MemoryGrantFeedbackCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteMemoryGrantFeedbackCatalogEntry(
            uint64_t grant_key_hash,
            ErrorContext* ctx = nullptr) -> Status;

        auto appendPageAuditFindingCatalogEntry(PageAuditFindingCatalogInfo& info,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getPageAuditFindingCatalogEntry(const ID& finding_id,
                                             PageAuditFindingCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listPageAuditFindingCatalogEntries(std::vector<PageAuditFindingCatalogInfo>& rows_out,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto appendShadowCaptureManifestCatalogEntry(ShadowCaptureManifestCatalogInfo& info,
                                                     ErrorContext* ctx = nullptr) -> Status;
        auto getShadowCaptureManifestCatalogEntry(const ID& manifest_id,
                                                  ShadowCaptureManifestCatalogInfo& info_out,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto listShadowCaptureManifestCatalogEntries(
            std::vector<ShadowCaptureManifestCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;

        auto appendForensicSnapshotCapsuleCatalogEntry(ForensicSnapshotCapsuleCatalogInfo& info,
                                                       ErrorContext* ctx = nullptr) -> Status;
        auto getForensicSnapshotCapsuleCatalogEntry(const ID& capsule_id,
                                                    ForensicSnapshotCapsuleCatalogInfo& info_out,
                                                    ErrorContext* ctx = nullptr) -> Status;
        auto listForensicSnapshotCapsuleCatalogEntries(
            std::vector<ForensicSnapshotCapsuleCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;

        // ============================================================================
        // Canonical runtime context catalog operations (CAT-019)
        // ============================================================================

        auto upsertRuntimeConnectionCatalogEntry(const RuntimeConnectionCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getRuntimeConnectionCatalogEntry(const ID& connection_id,
                                              RuntimeConnectionCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto listRuntimeConnectionCatalogEntries(
            const ID& database_id,
            std::vector<RuntimeConnectionCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteRuntimeConnectionCatalogEntry(const ID& connection_id,
                                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertRuntimeTransactionCatalogEntry(const RuntimeTransactionCatalogInfo& info,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto getRuntimeTransactionCatalogEntry(uint64_t txid,
                                               RuntimeTransactionCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listRuntimeTransactionCatalogEntries(
            const ID& database_id,
            std::vector<RuntimeTransactionCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteRuntimeTransactionCatalogEntry(uint64_t txid,
                                                  ErrorContext* ctx = nullptr) -> Status;

        auto upsertCheckpointRunCatalogEntry(const CheckpointRunCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getCheckpointRunCatalogEntry(const ID& checkpoint_run_uuid,
                                          CheckpointRunCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getLatestCheckpointRunCatalogEntry(CheckpointRunCatalogInfo& info_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto listCheckpointRunCatalogEntries(
            std::vector<CheckpointRunCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;

        auto upsertRecoveryRunCatalogEntry(const RecoveryRunCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getRecoveryRunCatalogEntry(const ID& recovery_run_uuid,
                                        RecoveryRunCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto getLatestRecoveryRunCatalogEntry(RecoveryRunCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto listRecoveryRunCatalogEntries(
            std::vector<RecoveryRunCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;

        auto appendSweepCursorStateCatalogEntry(SweepCursorStateCatalogInfo& info,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto listSweepCursorStateCatalogEntries(
            std::vector<SweepCursorStateCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;

        auto upsertWritebackIncidentCatalogEntry(const WritebackIncidentCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getWritebackIncidentCatalogEntry(const ID& writeback_incident_uuid,
                                              WritebackIncidentCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getOpenWritebackIncidentCatalogEntry(WritebackIncidentCatalogInfo& info_out,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto listWritebackIncidentCatalogEntries(
            std::vector<WritebackIncidentCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;

        auto appendRecoveryIncidentCatalogEntry(RecoveryIncidentCatalogInfo& info,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto listRecoveryIncidentCatalogEntries(
            std::vector<RecoveryIncidentCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;

        // ============================================================================
        // Canonical security extension and PKI/crypto catalog operations (CAT-020)
        // ============================================================================

        auto upsertPrincipalAccountCatalogEntry(const PrincipalAccountCatalogInfo& info,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getPrincipalAccountCatalogEntry(const ID& account_id,
                                             PrincipalAccountCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listPrincipalAccountCatalogEntries(std::vector<PrincipalAccountCatalogInfo>& rows_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto deletePrincipalAccountCatalogEntry(const ID& account_id,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto resolvePrincipalAccount(const PrincipalResolutionRequest& request,
                                     PrincipalAccountCatalogInfo& info_out,
                                     ErrorContext* ctx = nullptr) -> Status;

        auto upsertAccountCredentialCatalogEntry(const AccountCredentialCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getAccountCredentialCatalogEntry(const ID& credential_id,
                                              AccountCredentialCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto listAccountCredentialCatalogEntries(const ID& account_id,
                                                 std::vector<AccountCredentialCatalogInfo>& rows_out,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto deleteAccountCredentialCatalogEntry(const ID& credential_id,
                                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertAccountProfileBindingCatalogEntry(const AccountProfileBindingCatalogInfo& info,
                                                     ErrorContext* ctx = nullptr) -> Status;
        auto getAccountProfileBindingCatalogEntry(const ID& binding_id,
                                                  AccountProfileBindingCatalogInfo& info_out,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto listAccountProfileBindingCatalogEntries(
            const ID& account_id,
            std::vector<AccountProfileBindingCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteAccountProfileBindingCatalogEntry(const ID& binding_id,
                                                     ErrorContext* ctx = nullptr) -> Status;

        auto upsertAuthProviderCatalogEntry(const AuthProviderCatalogInfo& info,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getAuthProviderCatalogEntry(const ID& provider_id,
                                         AuthProviderCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listAuthProviderCatalogEntries(std::vector<AuthProviderCatalogInfo>& rows_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto deleteAuthProviderCatalogEntry(const ID& provider_id,
                                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertAuthPolicyCatalogEntry(const AuthPolicyCatalogInfo& info,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getAuthPolicyCatalogEntry(const ID& policy_id,
                                       AuthPolicyCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listAuthPolicyCatalogEntries(std::vector<AuthPolicyCatalogInfo>& rows_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteAuthPolicyCatalogEntry(const ID& policy_id,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertMfaPolicyCatalogEntry(const MfaPolicyCatalogInfo& info,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto getMfaPolicyCatalogEntry(const ID& mfa_policy_id,
                                      MfaPolicyCatalogInfo& info_out,
                                      ErrorContext* ctx = nullptr) -> Status;
        auto listMfaPolicyCatalogEntries(std::vector<MfaPolicyCatalogInfo>& rows_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto deleteMfaPolicyCatalogEntry(const ID& mfa_policy_id,
                                         ErrorContext* ctx = nullptr) -> Status;

        auto upsertMfaEnrollmentCatalogEntry(const MfaEnrollmentCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getMfaEnrollmentCatalogEntry(const ID& enrollment_id,
                                          MfaEnrollmentCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listMfaEnrollmentCatalogEntries(const ID& account_id,
                                             std::vector<MfaEnrollmentCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteMfaEnrollmentCatalogEntry(const ID& enrollment_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertMfaRecoveryCodeCatalogEntry(const MfaRecoveryCodeCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getMfaRecoveryCodeCatalogEntry(const ID& recovery_id,
                                            MfaRecoveryCodeCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listMfaRecoveryCodeCatalogEntries(const ID& account_id,
                                               std::vector<MfaRecoveryCodeCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteMfaRecoveryCodeCatalogEntry(const ID& recovery_id,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto consumeMfaRecoveryCode(const ID& account_id,
                                    const std::array<uint8_t, 32>& code_hash,
                                    bool allow_break_glass,
                                    MfaRecoveryCodeCatalogInfo& consumed_out,
                                    ErrorContext* ctx = nullptr) -> Status;

        auto upsertAuthAttemptLogCatalogEntry(const AuthAttemptLogCatalogInfo& info,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getAuthAttemptLogCatalogEntry(const ID& attempt_id,
                                           AuthAttemptLogCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listAuthAttemptLogCatalogEntries(
            const ID& account_id,
            std::vector<AuthAttemptLogCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteAuthAttemptLogCatalogEntry(const ID& attempt_id,
                                              ErrorContext* ctx = nullptr) -> Status;

        auto upsertConnectionRuleCatalogEntry(const ConnectionRuleCatalogInfo& info,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getConnectionRuleCatalogEntry(const ID& rule_id,
                                           ConnectionRuleCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listConnectionRuleCatalogEntries(
            const std::string& profile_scope,
            std::vector<ConnectionRuleCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteConnectionRuleCatalogEntry(const ID& rule_id,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getConnectionRuleEpochCatalogEntry(
            const std::string& profile_scope,
            ConnectionRuleEpochCatalogInfo& info_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto evaluateConnectionRuleChain(const ConnectionRuleEvaluationRequest& request,
                                         ConnectionRuleEvaluationDecision& decision_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto evaluateAuthProviderRuntime(const AuthProviderRuntimeRequest& request,
                                         AuthProviderRuntimeDecision& decision_out,
                                         ErrorContext* ctx = nullptr) -> Status;

        auto upsertAclCommandCatalogEntry(const AclCommandCatalogInfo& info,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getAclCommandCatalogEntry(const std::string& command_name,
                                       AclCommandCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listAclCommandCatalogEntries(std::vector<AclCommandCatalogInfo>& rows_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteAclCommandCatalogEntry(const std::string& command_name,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertAclRuleCatalogEntry(const AclRuleCatalogInfo& info,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto getAclRuleCatalogEntry(const ID& acl_rule_id,
                                    AclRuleCatalogInfo& info_out,
                                    ErrorContext* ctx = nullptr) -> Status;
        auto listAclRuleCatalogEntries(std::vector<AclRuleCatalogInfo>& rows_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto deleteAclRuleCatalogEntry(const ID& acl_rule_id,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto evaluateAclCommandPolicy(const AclEvaluationRequest& request,
                                      AclEvaluationDecision& decision_out,
                                      ErrorContext* ctx = nullptr) -> Status;

        auto upsertDocumentPolicyCatalogEntry(const DocumentPolicyCatalogInfo& info,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getDocumentPolicyCatalogEntry(const ID& policy_id,
                                           DocumentPolicyCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listDocumentPolicyCatalogEntries(std::vector<DocumentPolicyCatalogInfo>& rows_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto deleteDocumentPolicyCatalogEntry(const ID& policy_id,
                                              ErrorContext* ctx = nullptr) -> Status;

        auto upsertTenantBindingCatalogEntry(const TenantBindingCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getTenantBindingCatalogEntry(const ID& binding_id,
                                          TenantBindingCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listTenantBindingCatalogEntries(std::vector<TenantBindingCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteTenantBindingCatalogEntry(const ID& binding_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto evaluateDocumentReadPolicy(const DocumentAuthorizationRequest& request,
                                        DocumentAuthorizationDecision& decision_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto evaluateDocumentWritePolicy(const DocumentAuthorizationRequest& request,
                                         DocumentAuthorizationDecision& decision_out,
                                         ErrorContext* ctx = nullptr) -> Status;

        auto upsertGraphPrivilegeCatalogEntry(const GraphPrivilegeCatalogInfo& info,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getGraphPrivilegeCatalogEntry(const ID& graph_priv_id,
                                           GraphPrivilegeCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listGraphPrivilegeCatalogEntries(std::vector<GraphPrivilegeCatalogInfo>& rows_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto deleteGraphPrivilegeCatalogEntry(const ID& graph_priv_id,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto evaluateGraphPrivilegePolicy(const GraphAuthorizationRequest& request,
                                          GraphAuthorizationDecision& decision_out,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertTokenCatalogEntry(const TokenCatalogInfo& info,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto getTokenCatalogEntry(const ID& token_id,
                                  TokenCatalogInfo& info_out,
                                  ErrorContext* ctx = nullptr) -> Status;
        auto listTokenCatalogEntries(std::vector<TokenCatalogInfo>& rows_out,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto deleteTokenCatalogEntry(const ID& token_id,
                                     ErrorContext* ctx = nullptr) -> Status;

        auto upsertTokenScopeEntryCatalogEntry(const TokenScopeEntryCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getTokenScopeEntryCatalogEntry(const ID& scope_id,
                                            TokenScopeEntryCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listTokenScopeEntryCatalogEntries(const ID& token_id,
                                               std::vector<TokenScopeEntryCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteTokenScopeEntryCatalogEntry(const ID& scope_id,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto validateTokenScope(const TokenValidationRequest& request,
                                TokenValidationDecision& decision_out,
                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertQuotaProfileCatalogEntry(const QuotaProfileCatalogInfo& info,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getQuotaProfileCatalogEntry(const ID& quota_profile_id,
                                         QuotaProfileCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listQuotaProfileCatalogEntries(std::vector<QuotaProfileCatalogInfo>& rows_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto deleteQuotaProfileCatalogEntry(const ID& quota_profile_id,
                                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertQuotaBindingCatalogEntry(const QuotaBindingCatalogInfo& info,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getQuotaBindingCatalogEntry(const ID& binding_id,
                                         QuotaBindingCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listQuotaBindingCatalogEntries(std::vector<QuotaBindingCatalogInfo>& rows_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto deleteQuotaBindingCatalogEntry(const ID& binding_id,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto evaluateQuotaPolicy(const QuotaEvaluationRequest& request,
                                 QuotaEvaluationDecision& decision_out,
                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertSettingsProfileCatalogEntry(const SettingsProfileCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getSettingsProfileCatalogEntry(const ID& settings_profile_id,
                                            SettingsProfileCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listSettingsProfileCatalogEntries(std::vector<SettingsProfileCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteSettingsProfileCatalogEntry(const ID& settings_profile_id,
                                               ErrorContext* ctx = nullptr) -> Status;

        auto upsertSettingsBindingCatalogEntry(const SettingsBindingCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getSettingsBindingCatalogEntry(const ID& binding_id,
                                            SettingsBindingCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listSettingsBindingCatalogEntries(std::vector<SettingsBindingCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteSettingsBindingCatalogEntry(const ID& binding_id,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto resolveSettingsPolicy(const SettingsResolutionRequest& request,
                                   SettingsResolutionDecision& decision_out,
                                   ErrorContext* ctx = nullptr) -> Status;

        auto upsertConfigKeyCatalogEntry(const ConfigKeyCatalogInfo& info,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto getConfigKeyCatalogEntry(uint32_t key_id,
                                      ConfigKeyCatalogInfo& info_out,
                                      ErrorContext* ctx = nullptr) -> Status;
        auto getConfigKeyCatalogEntryByName(const std::string& key_name,
                                            ConfigKeyCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listConfigKeyCatalogEntries(std::vector<ConfigKeyCatalogInfo>& rows_out,
                                         ErrorContext* ctx = nullptr) -> Status;

        auto upsertConfigValueCatalogEntry(const ConfigValueCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getConfigValueCatalogEntry(uint32_t key_id,
                                        const ID* scope_uuid,
                                        ConfigValueCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listConfigValueCatalogEntries(std::vector<ConfigValueCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteConfigValueCatalogEntry(uint32_t key_id,
                                           const ID* scope_uuid,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto appendConfigChangeLogCatalogEntry(const ConfigChangeLogCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listConfigChangeLogCatalogEntries(
            std::vector<ConfigChangeLogCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;

        auto upsertListenerProfileCatalogEntry(const ListenerProfileCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getListenerProfileCatalogEntry(const ID& listener_profile_id,
                                            ListenerProfileCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listListenerProfileCatalogEntries(std::vector<ListenerProfileCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteListenerProfileCatalogEntry(const ID& listener_profile_id,
                                               ErrorContext* ctx = nullptr) -> Status;

        auto upsertListenerBindingCatalogEntry(const ListenerBindingCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getListenerBindingCatalogEntry(const ID& listener_binding_id,
                                            ListenerBindingCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listListenerBindingCatalogEntries(std::vector<ListenerBindingCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteListenerBindingCatalogEntry(const ID& listener_binding_id,
                                               ErrorContext* ctx = nullptr) -> Status;

        auto upsertListenerEmulationBindingCatalogEntry(
            const ListenerEmulationBindingCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getListenerEmulationBindingCatalogEntry(
            const ID& listener_emulation_binding_id,
            ListenerEmulationBindingCatalogInfo& info_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto listListenerEmulationBindingCatalogEntries(
            std::vector<ListenerEmulationBindingCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteListenerEmulationBindingCatalogEntry(
            const ID& listener_emulation_binding_id,
            ErrorContext* ctx = nullptr) -> Status;

        auto upsertParserPoolPolicyCatalogEntry(const ParserPoolPolicyCatalogInfo& info,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getParserPoolPolicyCatalogEntry(const ID& parser_pool_policy_id,
                                             ParserPoolPolicyCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listParserPoolPolicyCatalogEntries(std::vector<ParserPoolPolicyCatalogInfo>& rows_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto deleteParserPoolPolicyCatalogEntry(const ID& parser_pool_policy_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertListenerRuntimeTargetCatalogEntry(
            const ListenerRuntimeTargetCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getListenerRuntimeTargetCatalogEntry(const ID& listener_runtime_target_id,
                                                  ListenerRuntimeTargetCatalogInfo& info_out,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto listListenerRuntimeTargetCatalogEntries(
            std::vector<ListenerRuntimeTargetCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteListenerRuntimeTargetCatalogEntry(const ID& listener_runtime_target_id,
                                                     ErrorContext* ctx = nullptr) -> Status;

        auto upsertListenerGenerationRecordCatalogEntry(
            const ListenerGenerationRecordCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getListenerGenerationRecordCatalogEntry(const ID& listener_generation_id,
                                                     ListenerGenerationRecordCatalogInfo& info_out,
                                                     ErrorContext* ctx = nullptr) -> Status;
        auto listListenerGenerationRecordCatalogEntries(
            std::vector<ListenerGenerationRecordCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteListenerGenerationRecordCatalogEntry(const ID& listener_generation_id,
                                                        ErrorContext* ctx = nullptr) -> Status;

        auto upsertAuthMappingCatalogEntry(const AuthMappingCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getAuthMappingCatalogEntry(const ID& mapping_id,
                                        AuthMappingCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listAuthMappingCatalogEntries(std::vector<AuthMappingCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteAuthMappingCatalogEntry(const ID& mapping_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertRoleSettingCatalogEntry(const RoleSettingCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getRoleSettingCatalogEntry(const ID& role_setting_id,
                                        RoleSettingCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listRoleSettingCatalogEntries(const ID& role_id,
                                           std::vector<RoleSettingCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteRoleSettingCatalogEntry(const ID& role_setting_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertSecurityLabelCatalogEntry(const SecurityLabelCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getSecurityLabelCatalogEntry(const ID& security_label_id,
                                          SecurityLabelCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listSecurityLabelCatalogEntries(const ID& object_id,
                                             std::vector<SecurityLabelCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteSecurityLabelCatalogEntry(const ID& security_label_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertSecurityClassCatalogEntry(const SecurityClassCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getSecurityClassCatalogEntry(const ID& security_class_id,
                                          SecurityClassCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listSecurityClassCatalogEntries(std::vector<SecurityClassCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteSecurityClassCatalogEntry(const ID& security_class_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertCertRegistryCatalogEntry(const CertRegistryCatalogInfo& info,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getCertRegistryCatalogEntry(const ID& cert_id,
                                         CertRegistryCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listCertRegistryCatalogEntries(std::vector<CertRegistryCatalogInfo>& rows_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto deleteCertRegistryCatalogEntry(const ID& cert_id,
                                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertTrustAnchorCatalogEntry(const TrustAnchorCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getTrustAnchorCatalogEntry(const ID& anchor_id,
                                        TrustAnchorCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listTrustAnchorCatalogEntries(std::vector<TrustAnchorCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteTrustAnchorCatalogEntry(const ID& anchor_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertPrivateKeyStoreCatalogEntry(const PrivateKeyStoreCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getPrivateKeyStoreCatalogEntry(const ID& key_id,
                                            PrivateKeyStoreCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listPrivateKeyStoreCatalogEntries(std::vector<PrivateKeyStoreCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deletePrivateKeyStoreCatalogEntry(const ID& key_id,
                                               ErrorContext* ctx = nullptr) -> Status;

        auto upsertChannelCertBindingCatalogEntry(const ChannelCertBindingCatalogInfo& info,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto getChannelCertBindingCatalogEntry(const ID& binding_id,
                                               ChannelCertBindingCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listChannelCertBindingCatalogEntries(
            std::vector<ChannelCertBindingCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteChannelCertBindingCatalogEntry(const ID& binding_id,
                                                  ErrorContext* ctx = nullptr) -> Status;

        auto upsertCertRevocationCatalogEntry(const CertRevocationCatalogInfo& info,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getCertRevocationCatalogEntry(const ID& revocation_id,
                                           CertRevocationCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listCertRevocationCatalogEntries(std::vector<CertRevocationCatalogInfo>& rows_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto deleteCertRevocationCatalogEntry(const ID& revocation_id,
                                              ErrorContext* ctx = nullptr) -> Status;

        auto upsertPkiDistributionStateCatalogEntry(const PkiDistributionStateCatalogInfo& info,
                                                    ErrorContext* ctx = nullptr) -> Status;
        auto getPkiDistributionStateCatalogEntry(const ID& distribution_id,
                                                 PkiDistributionStateCatalogInfo& info_out,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto listPkiDistributionStateCatalogEntries(
            std::vector<PkiDistributionStateCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deletePkiDistributionStateCatalogEntry(const ID& distribution_id,
                                                    ErrorContext* ctx = nullptr) -> Status;

        auto upsertTrustAnchorRolloverCatalogEntry(const TrustAnchorRolloverCatalogInfo& info,
                                                   ErrorContext* ctx = nullptr) -> Status;
        auto getTrustAnchorRolloverCatalogEntry(const ID& rollover_id,
                                                TrustAnchorRolloverCatalogInfo& info_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto listTrustAnchorRolloverCatalogEntries(
            std::vector<TrustAnchorRolloverCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteTrustAnchorRolloverCatalogEntry(const ID& rollover_id,
                                                   ErrorContext* ctx = nullptr) -> Status;

        auto upsertNodeCatalogEntry(const NodeCatalogInfo& info,
                                    ErrorContext* ctx = nullptr) -> Status;
        auto getNodeCatalogEntry(const ID& node_id,
                                 NodeCatalogInfo& info_out,
                                 ErrorContext* ctx = nullptr) -> Status;
        auto listNodeCatalogEntries(const ID& cluster_id,
                                    std::vector<NodeCatalogInfo>& rows_out,
                                    ErrorContext* ctx = nullptr) -> Status;
        auto deleteNodeCatalogEntry(const ID& node_id,
                                    ErrorContext* ctx = nullptr) -> Status;

        auto upsertNodeRoleBindingCatalogEntry(const NodeRoleBindingCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getNodeRoleBindingCatalogEntry(const ID& binding_id,
                                            NodeRoleBindingCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listNodeRoleBindingCatalogEntries(const ID& node_id,
                                               std::vector<NodeRoleBindingCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteNodeRoleBindingCatalogEntry(const ID& binding_id,
                                               ErrorContext* ctx = nullptr) -> Status;

        auto upsertNodeServiceCatalogEntry(const NodeServiceCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getNodeServiceCatalogEntry(const ID& service_id,
                                        NodeServiceCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listNodeServiceCatalogEntries(const ID& node_id,
                                           std::vector<NodeServiceCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteNodeServiceCatalogEntry(const ID& service_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertNodeCapabilityCatalogEntry(const NodeCapabilityCatalogInfo& info,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getNodeCapabilityCatalogEntry(const ID& capability_id,
                                           NodeCapabilityCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listNodeCapabilityCatalogEntries(const ID& node_id,
                                              std::vector<NodeCapabilityCatalogInfo>& rows_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto deleteNodeCapabilityCatalogEntry(const ID& capability_id,
                                              ErrorContext* ctx = nullptr) -> Status;

        auto upsertClockPolicyCatalogEntry(const ClockPolicyCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getClockPolicyCatalogEntry(const ID& clock_policy_id,
                                        ClockPolicyCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listClockPolicyCatalogEntries(std::vector<ClockPolicyCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteClockPolicyCatalogEntry(const ID& clock_policy_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertClockSourceCatalogEntry(const ClockSourceCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getClockSourceCatalogEntry(const ID& clock_source_id,
                                        ClockSourceCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listClockSourceCatalogEntries(const ID& clock_policy_id,
                                           std::vector<ClockSourceCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteClockSourceCatalogEntry(const ID& clock_source_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertNodeClockStateCatalogEntry(const NodeClockStateCatalogInfo& info,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getNodeClockStateCatalogEntry(const ID& node_clock_state_id,
                                           NodeClockStateCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listNodeClockStateCatalogEntries(const ID& node_id,
                                              std::vector<NodeClockStateCatalogInfo>& rows_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto deleteNodeClockStateCatalogEntry(const ID& node_clock_state_id,
                                              ErrorContext* ctx = nullptr) -> Status;

        auto upsertClockViolationEventCatalogEntry(const ClockViolationEventCatalogInfo& info,
                                                   ErrorContext* ctx = nullptr) -> Status;
        auto getClockViolationEventCatalogEntry(const ID& clock_violation_event_id,
                                                ClockViolationEventCatalogInfo& info_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto listClockViolationEventCatalogEntries(const ID& node_id,
                                                   std::vector<ClockViolationEventCatalogInfo>& rows_out,
                                                   ErrorContext* ctx = nullptr) -> Status;
        auto deleteClockViolationEventCatalogEntry(const ID& clock_violation_event_id,
                                                   ErrorContext* ctx = nullptr) -> Status;

        auto upsertWorkloadClassCatalogEntry(const WorkloadClassCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getWorkloadClassCatalogEntry(const ID& class_id,
                                          WorkloadClassCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listWorkloadClassCatalogEntries(std::vector<WorkloadClassCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteWorkloadClassCatalogEntry(const ID& class_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertWorkloadRouteCatalogEntry(const WorkloadRouteCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getWorkloadRouteCatalogEntry(const ID& route_id,
                                          WorkloadRouteCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listWorkloadRouteCatalogEntries(const ID& class_id,
                                             std::vector<WorkloadRouteCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteWorkloadRouteCatalogEntry(const ID& route_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertAdmissionPolicyCatalogEntry(const AdmissionPolicyCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getAdmissionPolicyCatalogEntry(const ID& policy_id,
                                            AdmissionPolicyCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listAdmissionPolicyCatalogEntries(std::vector<AdmissionPolicyCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteAdmissionPolicyCatalogEntry(const ID& policy_id,
                                               ErrorContext* ctx = nullptr) -> Status;

        auto upsertAdmissionBindingCatalogEntry(const AdmissionBindingCatalogInfo& info,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getAdmissionBindingCatalogEntry(const ID& binding_id,
                                             AdmissionBindingCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listAdmissionBindingCatalogEntries(
            const ID& policy_id,
            std::vector<AdmissionBindingCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteAdmissionBindingCatalogEntry(const ID& binding_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertSloProfileCatalogEntry(const SloProfileCatalogInfo& info,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getSloProfileCatalogEntry(const ID& slo_profile_id,
                                       SloProfileCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listSloProfileCatalogEntries(std::vector<SloProfileCatalogInfo>& rows_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteSloProfileCatalogEntry(const ID& slo_profile_id,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertSloBindingCatalogEntry(const SloBindingCatalogInfo& info,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getSloBindingCatalogEntry(const ID& slo_binding_id,
                                       SloBindingCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listSloBindingCatalogEntries(const ID& slo_profile_id,
                                          std::vector<SloBindingCatalogInfo>& rows_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteSloBindingCatalogEntry(const ID& slo_binding_id,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertSloWindowCatalogEntry(const SloWindowCatalogInfo& info,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto getSloWindowCatalogEntry(const ID& slo_window_id,
                                      SloWindowCatalogInfo& info_out,
                                      ErrorContext* ctx = nullptr) -> Status;
        auto listSloWindowCatalogEntries(const ID& node_id,
                                         std::vector<SloWindowCatalogInfo>& rows_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto deleteSloWindowCatalogEntry(const ID& slo_window_id,
                                         ErrorContext* ctx = nullptr) -> Status;

        auto upsertSloBurnEventCatalogEntry(const SloBurnEventCatalogInfo& info,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getSloBurnEventCatalogEntry(const ID& slo_burn_event_id,
                                         SloBurnEventCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listSloBurnEventCatalogEntries(const ID& node_id,
                                            std::vector<SloBurnEventCatalogInfo>& rows_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto deleteSloBurnEventCatalogEntry(const ID& slo_burn_event_id,
                                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertAutoscalePolicyCatalogEntry(const AutoscalePolicyCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getAutoscalePolicyCatalogEntry(const ID& autoscale_policy_id,
                                            AutoscalePolicyCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listAutoscalePolicyCatalogEntries(std::vector<AutoscalePolicyCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteAutoscalePolicyCatalogEntry(const ID& autoscale_policy_id,
                                               ErrorContext* ctx = nullptr) -> Status;

        auto upsertAutoscaleActionCatalogEntry(const AutoscaleActionCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getAutoscaleActionCatalogEntry(const ID& autoscale_action_id,
                                            AutoscaleActionCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listAutoscaleActionCatalogEntries(const ClusterNodeRole role,
                                               std::vector<AutoscaleActionCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteAutoscaleActionCatalogEntry(const ID& autoscale_action_id,
                                               ErrorContext* ctx = nullptr) -> Status;

        auto upsertAdmissionTuningEventCatalogEntry(const AdmissionTuningEventCatalogInfo& info,
                                                    ErrorContext* ctx = nullptr) -> Status;
        auto getAdmissionTuningEventCatalogEntry(const ID& admission_tuning_event_id,
                                                 AdmissionTuningEventCatalogInfo& info_out,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto listAdmissionTuningEventCatalogEntries(
            const ClusterNodeRole role,
            std::vector<AdmissionTuningEventCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteAdmissionTuningEventCatalogEntry(const ID& admission_tuning_event_id,
                                                    ErrorContext* ctx = nullptr) -> Status;

        auto upsertClusterPolicyCatalogEntry(const ClusterPolicyCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getClusterPolicyCatalogEntry(const ID& policy_id,
                                          ClusterPolicyCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listClusterPolicyCatalogEntries(const ID& cluster_id,
                                             std::vector<ClusterPolicyCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteClusterPolicyCatalogEntry(const ID& policy_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertFailureDetectorCatalogEntry(const FailureDetectorCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getFailureDetectorCatalogEntry(const ID& detector_id,
                                            FailureDetectorCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listFailureDetectorCatalogEntries(const ID& cluster_id,
                                               std::vector<FailureDetectorCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteFailureDetectorCatalogEntry(const ID& detector_id,
                                               ErrorContext* ctx = nullptr) -> Status;

        auto upsertAlertRuleCatalogEntry(const AlertRuleCatalogInfo& info,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto getAlertRuleCatalogEntry(const ID& rule_id,
                                      AlertRuleCatalogInfo& info_out,
                                      ErrorContext* ctx = nullptr) -> Status;
        auto listAlertRuleCatalogEntries(std::vector<AlertRuleCatalogInfo>& rows_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto deleteAlertRuleCatalogEntry(const ID& rule_id,
                                         ErrorContext* ctx = nullptr) -> Status;

        auto upsertAlertTargetCatalogEntry(const AlertTargetCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getAlertTargetCatalogEntry(const ID& target_id,
                                        AlertTargetCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listAlertTargetCatalogEntries(std::vector<AlertTargetCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteAlertTargetCatalogEntry(const ID& target_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertAlertRouteCatalogEntry(const AlertRouteCatalogInfo& info,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getAlertRouteCatalogEntry(const ID& route_id,
                                       AlertRouteCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listAlertRouteCatalogEntries(const ID& rule_id,
                                          std::vector<AlertRouteCatalogInfo>& rows_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteAlertRouteCatalogEntry(const ID& route_id,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertAlertEventCatalogEntry(const AlertEventCatalogInfo& info,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getAlertEventCatalogEntry(const ID& event_id,
                                       AlertEventCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listAlertEventCatalogEntries(const ID& rule_id,
                                          std::vector<AlertEventCatalogInfo>& rows_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteAlertEventCatalogEntry(const ID& event_id,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertAlertAckCatalogEntry(const AlertAckCatalogInfo& info,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto getAlertAckCatalogEntry(const ID& ack_id,
                                     AlertAckCatalogInfo& info_out,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto listAlertAckCatalogEntries(const ID& event_id,
                                        std::vector<AlertAckCatalogInfo>& rows_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto deleteAlertAckCatalogEntry(const ID& ack_id,
                                        ErrorContext* ctx = nullptr) -> Status;

        auto upsertAlertSilenceCatalogEntry(const AlertSilenceCatalogInfo& info,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getAlertSilenceCatalogEntry(const ID& silence_id,
                                         AlertSilenceCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listAlertSilenceCatalogEntries(std::vector<AlertSilenceCatalogInfo>& rows_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto deleteAlertSilenceCatalogEntry(const ID& silence_id,
                                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertNetworkPartitionEventCatalogEntry(const NetworkPartitionEventCatalogInfo& info,
                                                     ErrorContext* ctx = nullptr) -> Status;
        auto getNetworkPartitionEventCatalogEntry(const ID& partition_id,
                                                  NetworkPartitionEventCatalogInfo& info_out,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto listNetworkPartitionEventCatalogEntries(
            const ID& cluster_id,
            std::vector<NetworkPartitionEventCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteNetworkPartitionEventCatalogEntry(const ID& partition_id,
                                                     ErrorContext* ctx = nullptr) -> Status;

        auto upsertNetworkPartitionMemberCatalogEntry(const NetworkPartitionMemberCatalogInfo& info,
                                                      ErrorContext* ctx = nullptr) -> Status;
        auto getNetworkPartitionMemberCatalogEntry(const ID& member_id,
                                                   NetworkPartitionMemberCatalogInfo& info_out,
                                                   ErrorContext* ctx = nullptr) -> Status;
        auto listNetworkPartitionMemberCatalogEntries(
            const ID& partition_id,
            std::vector<NetworkPartitionMemberCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteNetworkPartitionMemberCatalogEntry(const ID& member_id,
                                                      ErrorContext* ctx = nullptr) -> Status;

        auto upsertHealingPolicyCatalogEntry(const HealingPolicyCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getHealingPolicyCatalogEntry(const ID& policy_id,
                                          HealingPolicyCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listHealingPolicyCatalogEntries(std::vector<HealingPolicyCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteHealingPolicyCatalogEntry(const ID& policy_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertHealingActionCatalogEntry(const HealingActionCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getHealingActionCatalogEntry(const ID& action_id,
                                          HealingActionCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listHealingActionCatalogEntries(const ID& policy_id,
                                             std::vector<HealingActionCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteHealingActionCatalogEntry(const ID& action_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertHealingActionParamCatalogEntry(const HealingActionParamCatalogInfo& info,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto getHealingActionParamCatalogEntry(const ID& param_id,
                                               HealingActionParamCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listHealingActionParamCatalogEntries(
            const ID& action_id,
            std::vector<HealingActionParamCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteHealingActionParamCatalogEntry(const ID& param_id,
                                                  ErrorContext* ctx = nullptr) -> Status;

        auto upsertHealingRunCatalogEntry(const HealingRunCatalogInfo& info,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getHealingRunCatalogEntry(const ID& run_id,
                                       HealingRunCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listHealingRunCatalogEntries(const ID& policy_id,
                                          std::vector<HealingRunCatalogInfo>& rows_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteHealingRunCatalogEntry(const ID& run_id,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertHealingStepCatalogEntry(const HealingStepCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getHealingStepCatalogEntry(const ID& step_id,
                                        HealingStepCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listHealingStepCatalogEntries(const ID& run_id,
                                           std::vector<HealingStepCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteHealingStepCatalogEntry(const ID& step_id,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto runSecurityOperationsAutomation(const SecurityOperationsAutomationRequest& request,
                                             SecurityOperationsAutomationResult& result_out,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertJobTypeCatalogEntry(const JobTypeCatalogInfo& info,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto getJobTypeCatalogEntry(const ID& job_type_id,
                                    JobTypeCatalogInfo& info_out,
                                    ErrorContext* ctx = nullptr) -> Status;
        auto listJobTypeCatalogEntries(std::vector<JobTypeCatalogInfo>& rows_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto deleteJobTypeCatalogEntry(const ID& job_type_id,
                                       ErrorContext* ctx = nullptr) -> Status;

        auto upsertJobTypeParamCatalogEntry(const JobTypeParamCatalogInfo& info,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getJobTypeParamCatalogEntry(const ID& param_id,
                                         JobTypeParamCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listJobTypeParamCatalogEntries(const ID& job_type_id,
                                            std::vector<JobTypeParamCatalogInfo>& rows_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto deleteJobTypeParamCatalogEntry(const ID& param_id,
                                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertJobParamCatalogEntry(const JobParamCatalogInfo& info,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto getJobParamCatalogEntry(const ID& param_id,
                                     JobParamCatalogInfo& info_out,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto listJobParamCatalogEntries(const ID& job_id,
                                        std::vector<JobParamCatalogInfo>& rows_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto deleteJobParamCatalogEntry(const ID& param_id,
                                        ErrorContext* ctx = nullptr) -> Status;

        auto upsertJobScheduleCatalogEntry(const JobScheduleCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getJobScheduleCatalogEntry(const ID& schedule_id,
                                        JobScheduleCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listJobScheduleCatalogEntries(std::vector<JobScheduleCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteJobScheduleCatalogEntry(const ID& schedule_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertJobTypePolicyCatalogEntry(const JobTypePolicyCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getJobTypePolicyCatalogEntry(const ID& policy_id,
                                          JobTypePolicyCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listJobTypePolicyCatalogEntries(const ID& job_type_id,
                                             std::vector<JobTypePolicyCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteJobTypePolicyCatalogEntry(const ID& policy_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertRemoteConnectorCatalogEntry(const RemoteConnectorCatalogInfo& info,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto getRemoteConnectorCatalogEntry(const ID& remote_connector_id,
                                            RemoteConnectorCatalogInfo& info_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto listRemoteConnectorCatalogEntries(std::vector<RemoteConnectorCatalogInfo>& rows_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto deleteRemoteConnectorCatalogEntry(const ID& remote_connector_id,
                                               ErrorContext* ctx = nullptr) -> Status;

        auto upsertRemoteConnectorCapabilityCatalogEntry(
            const RemoteConnectorCapabilityCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getRemoteConnectorCapabilityCatalogEntry(const ID& capability_id,
                                                      RemoteConnectorCapabilityCatalogInfo& info_out,
                                                      ErrorContext* ctx = nullptr) -> Status;
        auto listRemoteConnectorCapabilityCatalogEntries(
            const ID& remote_connector_id,
            std::vector<RemoteConnectorCapabilityCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteRemoteConnectorCapabilityCatalogEntry(const ID& capability_id,
                                                         ErrorContext* ctx = nullptr) -> Status;

        auto upsertRemoteMetadataSnapshotCatalogEntry(const RemoteMetadataSnapshotCatalogInfo& info,
                                                      ErrorContext* ctx = nullptr) -> Status;
        auto getRemoteMetadataSnapshotCatalogEntry(const ID& snapshot_id,
                                                   RemoteMetadataSnapshotCatalogInfo& info_out,
                                                   ErrorContext* ctx = nullptr) -> Status;
        auto listRemoteMetadataSnapshotCatalogEntries(
            const ID& remote_connector_id,
            std::vector<RemoteMetadataSnapshotCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteRemoteMetadataSnapshotCatalogEntry(const ID& snapshot_id,
                                                      ErrorContext* ctx = nullptr) -> Status;

        auto upsertRemoteMetadataObjectCatalogEntry(const RemoteMetadataObjectCatalogInfo& info,
                                                    ErrorContext* ctx = nullptr) -> Status;
        auto getRemoteMetadataObjectCatalogEntry(const ID& remote_object_id,
                                                 RemoteMetadataObjectCatalogInfo& info_out,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto listRemoteMetadataObjectCatalogEntries(
            const ID& snapshot_id,
            std::vector<RemoteMetadataObjectCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteRemoteMetadataObjectCatalogEntry(const ID& remote_object_id,
                                                    ErrorContext* ctx = nullptr) -> Status;

        auto upsertRemoteMetadataColumnCatalogEntry(const RemoteMetadataColumnCatalogInfo& info,
                                                    ErrorContext* ctx = nullptr) -> Status;
        auto getRemoteMetadataColumnCatalogEntry(const ID& remote_column_id,
                                                 RemoteMetadataColumnCatalogInfo& info_out,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto listRemoteMetadataColumnCatalogEntries(
            const ID& remote_object_id,
            std::vector<RemoteMetadataColumnCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteRemoteMetadataColumnCatalogEntry(const ID& remote_column_id,
                                                    ErrorContext* ctx = nullptr) -> Status;

        auto upsertRemoteSchemaMappingCatalogEntry(const RemoteSchemaMappingCatalogInfo& info,
                                                   ErrorContext* ctx = nullptr) -> Status;
        auto getRemoteSchemaMappingCatalogEntry(const ID& schema_mapping_id,
                                                RemoteSchemaMappingCatalogInfo& info_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto listRemoteSchemaMappingCatalogEntries(
            const ID& remote_connector_id,
            std::vector<RemoteSchemaMappingCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteRemoteSchemaMappingCatalogEntry(const ID& schema_mapping_id,
                                                   ErrorContext* ctx = nullptr) -> Status;

        auto upsertRemotePassthroughPolicyCatalogEntry(
            const RemotePassthroughPolicyCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getRemotePassthroughPolicyCatalogEntry(const ID& remote_policy_id,
                                                    RemotePassthroughPolicyCatalogInfo& info_out,
                                                    ErrorContext* ctx = nullptr) -> Status;
        auto listRemotePassthroughPolicyCatalogEntries(
            const ID& remote_connector_id,
            std::vector<RemotePassthroughPolicyCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteRemotePassthroughPolicyCatalogEntry(const ID& remote_policy_id,
                                                       ErrorContext* ctx = nullptr) -> Status;

        auto upsertRemotePreparedStatementCatalogEntry(
            const RemotePreparedStatementCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getRemotePreparedStatementCatalogEntry(const ID& remote_prepared_id,
                                                    RemotePreparedStatementCatalogInfo& info_out,
                                                    ErrorContext* ctx = nullptr) -> Status;
        auto listRemotePreparedStatementCatalogEntries(
            const ID& session_id,
            std::vector<RemotePreparedStatementCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteRemotePreparedStatementCatalogEntry(const ID& remote_prepared_id,
                                                       ErrorContext* ctx = nullptr) -> Status;

        auto upsertRemoteTxnBindingCatalogEntry(const RemoteTxnBindingCatalogInfo& info,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getRemoteTxnBindingCatalogEntry(const ID& remote_txn_binding_id,
                                             RemoteTxnBindingCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listRemoteTxnBindingCatalogEntries(const ID& remote_connector_id,
                                                std::vector<RemoteTxnBindingCatalogInfo>& rows_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto deleteRemoteTxnBindingCatalogEntry(const ID& remote_txn_binding_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertRemoteExecutionAuditCatalogEntry(const RemoteExecutionAuditCatalogInfo& info,
                                                    ErrorContext* ctx = nullptr) -> Status;
        auto getRemoteExecutionAuditCatalogEntry(const ID& remote_exec_audit_id,
                                                 RemoteExecutionAuditCatalogInfo& info_out,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto listRemoteExecutionAuditCatalogEntries(
            const ID& remote_connector_id,
            std::vector<RemoteExecutionAuditCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteRemoteExecutionAuditCatalogEntry(const ID& remote_exec_audit_id,
                                                    ErrorContext* ctx = nullptr) -> Status;

        auto upsertRemoteErrorCatalogEntry(const RemoteErrorCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getRemoteErrorCatalogEntry(const ID& remote_error_id,
                                        RemoteErrorCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listRemoteErrorCatalogEntries(const ID& remote_connector_id,
                                           std::vector<RemoteErrorCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteRemoteErrorCatalogEntry(const ID& remote_error_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertExtensionCatalogEntry(const ExtensionCatalogInfo& info,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto getExtensionCatalogEntry(const ID& extension_id,
                                      ExtensionCatalogInfo& info_out,
                                      ErrorContext* ctx = nullptr) -> Status;
        auto listExtensionCatalogEntries(std::vector<ExtensionCatalogInfo>& rows_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto deleteExtensionCatalogEntry(const ID& extension_id,
                                         ErrorContext* ctx = nullptr) -> Status;

        auto upsertPublicationCatalogEntry(const PublicationCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getPublicationCatalogEntry(const ID& publication_id,
                                        PublicationCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listPublicationCatalogEntries(std::vector<PublicationCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deletePublicationCatalogEntry(const ID& publication_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertPublicationTableCatalogEntry(const PublicationTableCatalogInfo& info,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getPublicationTableCatalogEntry(const ID& publication_table_id,
                                             PublicationTableCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listPublicationTableCatalogEntries(const ID& publication_id,
                                                std::vector<PublicationTableCatalogInfo>& rows_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto deletePublicationTableCatalogEntry(const ID& publication_table_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertPublicationSchemaCatalogEntry(const PublicationSchemaCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getPublicationSchemaCatalogEntry(const ID& publication_schema_id,
                                              PublicationSchemaCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto listPublicationSchemaCatalogEntries(const ID& publication_id,
                                                 std::vector<PublicationSchemaCatalogInfo>& rows_out,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto deletePublicationSchemaCatalogEntry(const ID& publication_schema_id,
                                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertSubscriptionCatalogEntry(const SubscriptionCatalogInfo& info,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getSubscriptionCatalogEntry(const ID& subscription_id,
                                         SubscriptionCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listSubscriptionCatalogEntries(std::vector<SubscriptionCatalogInfo>& rows_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto deleteSubscriptionCatalogEntry(const ID& subscription_id,
                                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertSubscriptionTableCatalogEntry(const SubscriptionTableCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getSubscriptionTableCatalogEntry(const ID& subscription_table_id,
                                              SubscriptionTableCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto listSubscriptionTableCatalogEntries(const ID& subscription_id,
                                                 std::vector<SubscriptionTableCatalogInfo>& rows_out,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto deleteSubscriptionTableCatalogEntry(const ID& subscription_table_id,
                                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertReplicationChannelCatalogEntry(const ReplicationChannelCatalogInfo& info,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto getReplicationChannelCatalogEntry(const ID& replication_channel_id,
                                               ReplicationChannelCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listReplicationChannelCatalogEntries(std::vector<ReplicationChannelCatalogInfo>& rows_out,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto deleteReplicationChannelCatalogEntry(const ID& replication_channel_id,
                                                  ErrorContext* ctx = nullptr) -> Status;

        auto upsertReplicationChannelMemberCatalogEntry(const ReplicationChannelMemberCatalogInfo& info,
                                                        ErrorContext* ctx = nullptr) -> Status;
        auto getReplicationChannelMemberCatalogEntry(const ID& channel_member_id,
                                                     ReplicationChannelMemberCatalogInfo& info_out,
                                                     ErrorContext* ctx = nullptr) -> Status;
        auto listReplicationChannelMemberCatalogEntries(
            const ID& replication_channel_id,
            std::vector<ReplicationChannelMemberCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteReplicationChannelMemberCatalogEntry(const ID& channel_member_id,
                                                        ErrorContext* ctx = nullptr) -> Status;

        auto upsertReplicationOriginCatalogEntry(const ReplicationOriginCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getReplicationOriginCatalogEntry(const ID& origin_id,
                                              ReplicationOriginCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto listReplicationOriginCatalogEntries(std::vector<ReplicationOriginCatalogInfo>& rows_out,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto deleteReplicationOriginCatalogEntry(const ID& origin_id,
                                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertReplicationCursorCatalogEntry(const ReplicationCursorCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getReplicationCursorCatalogEntry(const ID& replication_cursor_id,
                                              ReplicationCursorCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto listReplicationCursorCatalogEntries(
            const ID& replication_channel_id,
            std::vector<ReplicationCursorCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteReplicationCursorCatalogEntry(const ID& replication_cursor_id,
                                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertReplicationOriginProgressCatalogEntry(const ReplicationOriginProgressCatalogInfo& info,
                                                         ErrorContext* ctx = nullptr) -> Status;
        auto getReplicationOriginProgressCatalogEntry(const ID& origin_progress_id,
                                                      ReplicationOriginProgressCatalogInfo& info_out,
                                                      ErrorContext* ctx = nullptr) -> Status;
        auto listReplicationOriginProgressCatalogEntries(
            const ID& replication_channel_id,
            std::vector<ReplicationOriginProgressCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteReplicationOriginProgressCatalogEntry(const ID& origin_progress_id,
                                                         ErrorContext* ctx = nullptr) -> Status;

        auto upsertReplicationTxnBatchCatalogEntry(const ReplicationTxnBatchCatalogInfo& info,
                                                   ErrorContext* ctx = nullptr) -> Status;
        auto getReplicationTxnBatchCatalogEntry(const ID& replication_batch_id,
                                                ReplicationTxnBatchCatalogInfo& info_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto listReplicationTxnBatchCatalogEntries(
            const ID& replication_channel_id,
            std::vector<ReplicationTxnBatchCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteReplicationTxnBatchCatalogEntry(const ID& replication_batch_id,
                                                   ErrorContext* ctx = nullptr) -> Status;

        auto upsertReplicationApplyLogCatalogEntry(const ReplicationApplyLogCatalogInfo& info,
                                                   ErrorContext* ctx = nullptr) -> Status;
        auto getReplicationApplyLogCatalogEntry(const ID& replication_apply_log_id,
                                                ReplicationApplyLogCatalogInfo& info_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto listReplicationApplyLogCatalogEntries(
            const ID& replication_batch_id,
            std::vector<ReplicationApplyLogCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteReplicationApplyLogCatalogEntry(const ID& replication_apply_log_id,
                                                   ErrorContext* ctx = nullptr) -> Status;

        auto upsertReplicationRetryQueueCatalogEntry(const ReplicationRetryQueueCatalogInfo& info,
                                                     ErrorContext* ctx = nullptr) -> Status;
        auto getReplicationRetryQueueCatalogEntry(const ID& replication_retry_id,
                                                  ReplicationRetryQueueCatalogInfo& info_out,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto listReplicationRetryQueueCatalogEntries(
            std::vector<ReplicationRetryQueueCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteReplicationRetryQueueCatalogEntry(const ID& replication_retry_id,
                                                     ErrorContext* ctx = nullptr) -> Status;

        auto upsertReplicationConflictCatalogEntry(const ReplicationConflictCatalogInfo& info,
                                                   ErrorContext* ctx = nullptr) -> Status;
        auto getReplicationConflictCatalogEntry(const ID& replication_conflict_id,
                                                ReplicationConflictCatalogInfo& info_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto listReplicationConflictCatalogEntries(
            const ID& replication_channel_id,
            std::vector<ReplicationConflictCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteReplicationConflictCatalogEntry(const ID& replication_conflict_id,
                                                   ErrorContext* ctx = nullptr) -> Status;

        auto upsertReplicationSplitBrainEventCatalogEntry(
            const ReplicationSplitBrainEventCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getReplicationSplitBrainEventCatalogEntry(const ID& replication_split_brain_id,
                                                       ReplicationSplitBrainEventCatalogInfo& info_out,
                                                       ErrorContext* ctx = nullptr) -> Status;
        auto listReplicationSplitBrainEventCatalogEntries(
            const ID& replication_channel_id,
            std::vector<ReplicationSplitBrainEventCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteReplicationSplitBrainEventCatalogEntry(const ID& replication_split_brain_id,
                                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertReplicationErrorCatalogEntry(const ReplicationErrorCatalogInfo& info,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getReplicationErrorCatalogEntry(const ID& replication_error_id,
                                             ReplicationErrorCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listReplicationErrorCatalogEntries(
            const ID& replication_channel_id,
            std::vector<ReplicationErrorCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteReplicationErrorCatalogEntry(const ID& replication_error_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertClusterFabricLinkCatalogEntry(const ClusterFabricLinkCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getClusterFabricLinkCatalogEntry(const ID& cluster_fabric_link_id,
                                              ClusterFabricLinkCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto listClusterFabricLinkCatalogEntries(std::vector<ClusterFabricLinkCatalogInfo>& rows_out,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto deleteClusterFabricLinkCatalogEntry(const ID& cluster_fabric_link_id,
                                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertClusterFabricSessionCatalogEntry(const ClusterFabricSessionCatalogInfo& info,
                                                    ErrorContext* ctx = nullptr) -> Status;
        auto getClusterFabricSessionCatalogEntry(const ID& cluster_fabric_session_id,
                                                 ClusterFabricSessionCatalogInfo& info_out,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto listClusterFabricSessionCatalogEntries(
            const ID& cluster_fabric_link_id,
            std::vector<ClusterFabricSessionCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteClusterFabricSessionCatalogEntry(const ID& cluster_fabric_session_id,
                                                    ErrorContext* ctx = nullptr) -> Status;

        auto upsertClusterFabricTxnCatalogEntry(const ClusterFabricTxnCatalogInfo& info,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getClusterFabricTxnCatalogEntry(const ID& cluster_fabric_txn_id,
                                             ClusterFabricTxnCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listClusterFabricTxnCatalogEntries(
            const ID& cluster_fabric_session_id,
            std::vector<ClusterFabricTxnCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteClusterFabricTxnCatalogEntry(const ID& cluster_fabric_txn_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertClusterFabricTaskCatalogEntry(const ClusterFabricTaskCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getClusterFabricTaskCatalogEntry(const ID& cluster_fabric_task_id,
                                              ClusterFabricTaskCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto listClusterFabricTaskCatalogEntries(
            const ID& cluster_fabric_link_id,
            std::vector<ClusterFabricTaskCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteClusterFabricTaskCatalogEntry(const ID& cluster_fabric_task_id,
                                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertClusterFabricTaskChunkCatalogEntry(const ClusterFabricTaskChunkCatalogInfo& info,
                                                      ErrorContext* ctx = nullptr) -> Status;
        auto getClusterFabricTaskChunkCatalogEntry(const ID& cluster_fabric_task_chunk_id,
                                                   ClusterFabricTaskChunkCatalogInfo& info_out,
                                                   ErrorContext* ctx = nullptr) -> Status;
        auto listClusterFabricTaskChunkCatalogEntries(
            const ID& cluster_fabric_task_id,
            std::vector<ClusterFabricTaskChunkCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteClusterFabricTaskChunkCatalogEntry(const ID& cluster_fabric_task_chunk_id,
                                                      ErrorContext* ctx = nullptr) -> Status;

        auto upsertClusterFabricEventCatalogEntry(const ClusterFabricEventCatalogInfo& info,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto getClusterFabricEventCatalogEntry(const ID& cluster_fabric_event_id,
                                               ClusterFabricEventCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listClusterFabricEventCatalogEntries(
            const ID& cluster_fabric_link_id,
            std::vector<ClusterFabricEventCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteClusterFabricEventCatalogEntry(const ID& cluster_fabric_event_id,
                                                  ErrorContext* ctx = nullptr) -> Status;

        auto upsertClusterFabricErrorCatalogEntry(const ClusterFabricErrorCatalogInfo& info,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto getClusterFabricErrorCatalogEntry(const ID& cluster_fabric_error_id,
                                               ClusterFabricErrorCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listClusterFabricErrorCatalogEntries(
            const ID& cluster_fabric_link_id,
            std::vector<ClusterFabricErrorCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteClusterFabricErrorCatalogEntry(const ID& cluster_fabric_error_id,
                                                  ErrorContext* ctx = nullptr) -> Status;

        auto upsertOlapWatermarkCatalogEntry(const OlapWatermarkCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getOlapWatermarkCatalogEntry(const ID& watermark_id,
                                          OlapWatermarkCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listOlapWatermarkCatalogEntries(std::vector<OlapWatermarkCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteOlapWatermarkCatalogEntry(const ID& watermark_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertOlapPartitionCatalogEntry(const OlapPartitionCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getOlapPartitionCatalogEntry(const ID& partition_id,
                                          OlapPartitionCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listOlapPartitionCatalogEntries(const ID& table_id,
                                             std::vector<OlapPartitionCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteOlapPartitionCatalogEntry(const ID& partition_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertOlapSegmentCatalogEntry(const OlapSegmentCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getOlapSegmentCatalogEntry(const ID& segment_id,
                                        OlapSegmentCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listOlapSegmentCatalogEntries(const ID& partition_id,
                                           std::vector<OlapSegmentCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteOlapSegmentCatalogEntry(const ID& segment_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertOlapIngestLogCatalogEntry(const OlapIngestLogCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getOlapIngestLogCatalogEntry(const ID& batch_id,
                                          OlapIngestLogCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listOlapIngestLogCatalogEntries(const ID& table_id,
                                             std::vector<OlapIngestLogCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteOlapIngestLogCatalogEntry(const ID& batch_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertCubeCatalogEntry(const CubeCatalogInfo& info,
                                    ErrorContext* ctx = nullptr) -> Status;
        auto getCubeCatalogEntry(const ID& cube_id,
                                 CubeCatalogInfo& info_out,
                                 ErrorContext* ctx = nullptr) -> Status;
        auto listCubeCatalogEntries(const ID& schema_id,
                                    std::vector<CubeCatalogInfo>& rows_out,
                                    ErrorContext* ctx = nullptr) -> Status;
        auto deleteCubeCatalogEntry(const ID& cube_id,
                                    ErrorContext* ctx = nullptr) -> Status;

        auto upsertCubeDimensionCatalogEntry(const CubeDimensionCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getCubeDimensionCatalogEntry(const ID& dimension_id,
                                          CubeDimensionCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listCubeDimensionCatalogEntries(const ID& cube_id,
                                             std::vector<CubeDimensionCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteCubeDimensionCatalogEntry(const ID& dimension_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertCubeLevelCatalogEntry(const CubeLevelCatalogInfo& info,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto getCubeLevelCatalogEntry(const ID& level_id,
                                      CubeLevelCatalogInfo& info_out,
                                      ErrorContext* ctx = nullptr) -> Status;
        auto listCubeLevelCatalogEntries(const ID& dimension_id,
                                         std::vector<CubeLevelCatalogInfo>& rows_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto deleteCubeLevelCatalogEntry(const ID& level_id,
                                         ErrorContext* ctx = nullptr) -> Status;

        auto upsertCubeHierarchyCatalogEntry(const CubeHierarchyCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getCubeHierarchyCatalogEntry(const ID& hierarchy_id,
                                          CubeHierarchyCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listCubeHierarchyCatalogEntries(const ID& dimension_id,
                                             std::vector<CubeHierarchyCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteCubeHierarchyCatalogEntry(const ID& hierarchy_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertCubeHierarchyLevelCatalogEntry(const CubeHierarchyLevelCatalogInfo& info,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto getCubeHierarchyLevelCatalogEntry(const ID& hierarchy_level_id,
                                               CubeHierarchyLevelCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listCubeHierarchyLevelCatalogEntries(const ID& hierarchy_id,
                                                  std::vector<CubeHierarchyLevelCatalogInfo>& rows_out,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto deleteCubeHierarchyLevelCatalogEntry(const ID& hierarchy_level_id,
                                                  ErrorContext* ctx = nullptr) -> Status;

        auto upsertCubeMeasureCatalogEntry(const CubeMeasureCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getCubeMeasureCatalogEntry(const ID& measure_id,
                                        CubeMeasureCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listCubeMeasureCatalogEntries(const ID& cube_id,
                                           std::vector<CubeMeasureCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteCubeMeasureCatalogEntry(const ID& measure_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertCubeMaterializationCatalogEntry(const CubeMaterializationCatalogInfo& info,
                                                   ErrorContext* ctx = nullptr) -> Status;
        auto getCubeMaterializationCatalogEntry(const ID& materialization_id,
                                                CubeMaterializationCatalogInfo& info_out,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto listCubeMaterializationCatalogEntries(
            const ID& cube_id,
            std::vector<CubeMaterializationCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteCubeMaterializationCatalogEntry(const ID& materialization_id,
                                                   ErrorContext* ctx = nullptr) -> Status;

        auto upsertCubeRefreshPolicyCatalogEntry(const CubeRefreshPolicyCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getCubeRefreshPolicyCatalogEntry(const ID& policy_id,
                                              CubeRefreshPolicyCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto listCubeRefreshPolicyCatalogEntries(const ID& cube_id,
                                                 std::vector<CubeRefreshPolicyCatalogInfo>& rows_out,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto deleteCubeRefreshPolicyCatalogEntry(const ID& policy_id,
                                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertCubeJobCatalogEntry(const CubeJobCatalogInfo& info,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto getCubeJobCatalogEntry(const ID& job_id,
                                    CubeJobCatalogInfo& info_out,
                                    ErrorContext* ctx = nullptr) -> Status;
        auto listCubeJobCatalogEntries(const ID& cube_id,
                                       std::vector<CubeJobCatalogInfo>& rows_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto deleteCubeJobCatalogEntry(const ID& job_id,
                                       ErrorContext* ctx = nullptr) -> Status;

        auto upsertCubeJobStepCatalogEntry(const CubeJobStepCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getCubeJobStepCatalogEntry(const ID& step_id,
                                        CubeJobStepCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listCubeJobStepCatalogEntries(const ID& job_id,
                                           std::vector<CubeJobStepCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteCubeJobStepCatalogEntry(const ID& step_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertCubeStatsCatalogEntry(const CubeStatsCatalogInfo& info,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto getCubeStatsCatalogEntry(const ID& cube_id,
                                      CubeStatsCatalogInfo& info_out,
                                      ErrorContext* ctx = nullptr) -> Status;
        auto listCubeStatsCatalogEntries(std::vector<CubeStatsCatalogInfo>& rows_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto deleteCubeStatsCatalogEntry(const ID& cube_id,
                                         ErrorContext* ctx = nullptr) -> Status;

        auto upsertTsParserCatalogEntry(const TsParserCatalogInfo& info,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto getTsParserCatalogEntry(const ID& parser_id,
                                     TsParserCatalogInfo& info_out,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto listTsParserCatalogEntries(std::vector<TsParserCatalogInfo>& rows_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto deleteTsParserCatalogEntry(const ID& parser_id,
                                        ErrorContext* ctx = nullptr) -> Status;

        auto upsertTsTemplateCatalogEntry(const TsTemplateCatalogInfo& info,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getTsTemplateCatalogEntry(const ID& template_id,
                                       TsTemplateCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listTsTemplateCatalogEntries(std::vector<TsTemplateCatalogInfo>& rows_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteTsTemplateCatalogEntry(const ID& template_id,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertTsDictionaryCatalogEntry(const TsDictionaryCatalogInfo& info,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getTsDictionaryCatalogEntry(const ID& dictionary_id,
                                         TsDictionaryCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listTsDictionaryCatalogEntries(const ID& template_id,
                                            std::vector<TsDictionaryCatalogInfo>& rows_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto deleteTsDictionaryCatalogEntry(const ID& dictionary_id,
                                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertTsConfigCatalogEntry(const TsConfigCatalogInfo& info,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto getTsConfigCatalogEntry(const ID& config_id,
                                     TsConfigCatalogInfo& info_out,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto listTsConfigCatalogEntries(const ID& parser_id,
                                        std::vector<TsConfigCatalogInfo>& rows_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto deleteTsConfigCatalogEntry(const ID& config_id,
                                        ErrorContext* ctx = nullptr) -> Status;

        auto upsertTsConfigMapCatalogEntry(const TsConfigMapCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getTsConfigMapCatalogEntry(const ID& map_id,
                                        TsConfigMapCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listTsConfigMapCatalogEntries(const ID& config_id,
                                           std::vector<TsConfigMapCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteTsConfigMapCatalogEntry(const ID& map_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertBlobFilterCatalogEntry(const BlobFilterCatalogInfo& info,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getBlobFilterCatalogEntry(const ID& filter_id,
                                       BlobFilterCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listBlobFilterCatalogEntries(std::vector<BlobFilterCatalogInfo>& rows_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteBlobFilterCatalogEntry(const ID& filter_id,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertTriggerMessageCatalogEntry(const TriggerMessageCatalogInfo& info,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getTriggerMessageCatalogEntry(const ID& message_id,
                                           TriggerMessageCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listTriggerMessageCatalogEntries(const ID& trigger_id,
                                              std::vector<TriggerMessageCatalogInfo>& rows_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto deleteTriggerMessageCatalogEntry(const ID& message_id,
                                              ErrorContext* ctx = nullptr) -> Status;

        auto upsertColumnDropHistoryCatalogEntry(const ColumnDropHistoryCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getColumnDropHistoryCatalogEntry(const ID& history_id,
                                              ColumnDropHistoryCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto listColumnDropHistoryCatalogEntries(
            const ID& table_id,
            std::vector<ColumnDropHistoryCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteColumnDropHistoryCatalogEntry(const ID& history_id,
                                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertSblrModuleCatalogEntry(const SblrModuleCatalogInfo& info,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getSblrModuleCatalogEntry(const ID& module_id,
                                       SblrModuleCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listSblrModuleCatalogEntries(std::vector<SblrModuleCatalogInfo>& rows_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteSblrModuleCatalogEntry(const ID& module_id,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertSblrPlanCatalogEntry(const SblrPlanCatalogInfo& info,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto getSblrPlanCatalogEntry(const ID& plan_id,
                                     SblrPlanCatalogInfo& info_out,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto listSblrPlanCatalogEntries(const ID& module_id,
                                        std::vector<SblrPlanCatalogInfo>& rows_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto deleteSblrPlanCatalogEntry(const ID& plan_id,
                                        ErrorContext* ctx = nullptr) -> Status;

        auto upsertSblrPlanDependencyCatalogEntry(const SblrPlanDependencyCatalogInfo& info,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto getSblrPlanDependencyCatalogEntry(const ID& plan_id,
                                               const ID& object_id,
                                               SblrPlanDependencyCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listSblrPlanDependencyCatalogEntries(
            const ID& plan_id,
            std::vector<SblrPlanDependencyCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteSblrPlanDependencyCatalogEntry(const ID& plan_id,
                                                  const ID& object_id,
                                                  ErrorContext* ctx = nullptr) -> Status;

        auto upsertSblrStatementNormCatalogEntry(const SblrStatementNormCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getSblrStatementNormCatalogEntry(const ID& module_id,
                                              const ID& statement_id,
                                              SblrStatementNormCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto listSblrStatementNormCatalogEntries(
            const ID& module_id,
            std::vector<SblrStatementNormCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteSblrStatementNormCatalogEntry(const ID& module_id,
                                                 const ID& statement_id,
                                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertSblrArtifactCatalogEntry(const SblrArtifactCatalogInfo& info,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getSblrArtifactCatalogEntry(const ID& artifact_id,
                                         SblrArtifactCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listSblrArtifactCatalogEntries(const ID& module_id,
                                            std::vector<SblrArtifactCatalogInfo>& rows_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto deleteSblrArtifactCatalogEntry(const ID& artifact_id,
                                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertSblrArtifactStatsCatalogEntry(const SblrArtifactStatsCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getSblrArtifactStatsCatalogEntry(const ID& artifact_id,
                                              SblrArtifactStatsCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto listSblrArtifactStatsCatalogEntries(
            std::vector<SblrArtifactStatsCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteSblrArtifactStatsCatalogEntry(const ID& artifact_id,
                                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertSblrCompilerTargetCatalogEntry(const SblrCompilerTargetCatalogInfo& info,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto getSblrCompilerTargetCatalogEntry(const std::string& target_name,
                                               SblrCompilerTargetCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listSblrCompilerTargetCatalogEntries(
            std::vector<SblrCompilerTargetCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteSblrCompilerTargetCatalogEntry(const std::string& target_name,
                                                  ErrorContext* ctx = nullptr) -> Status;

        auto upsertSblrCompileQueueCatalogEntry(const SblrCompileQueueCatalogInfo& info,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getSblrCompileQueueCatalogEntry(const ID& queue_id,
                                             SblrCompileQueueCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listSblrCompileQueueCatalogEntries(
            const ID& module_id,
            std::vector<SblrCompileQueueCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteSblrCompileQueueCatalogEntry(const ID& queue_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertClusterCatalogEntry(const ClusterCatalogInfo& info,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto getClusterCatalogEntry(const ID& cluster_id,
                                    ClusterCatalogInfo& info_out,
                                    ErrorContext* ctx = nullptr) -> Status;
        auto listClusterCatalogEntries(std::vector<ClusterCatalogInfo>& rows_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto deleteClusterCatalogEntry(const ID& cluster_id,
                                       ErrorContext* ctx = nullptr) -> Status;

        auto upsertShardPolicyCatalogEntry(const ShardPolicyCatalogInfo& info,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto getShardPolicyCatalogEntry(const ID& policy_id,
                                        ShardPolicyCatalogInfo& info_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto listShardPolicyCatalogEntries(std::vector<ShardPolicyCatalogInfo>& rows_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto deleteShardPolicyCatalogEntry(const ID& policy_id,
                                           ErrorContext* ctx = nullptr) -> Status;

        auto upsertShardPolicyParamCatalogEntry(const ShardPolicyParamCatalogInfo& info,
                                                ErrorContext* ctx = nullptr) -> Status;
        auto getShardPolicyParamCatalogEntry(const ID& policy_param_id,
                                             ShardPolicyParamCatalogInfo& info_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto listShardPolicyParamCatalogEntries(
            const ID& policy_id,
            std::vector<ShardPolicyParamCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteShardPolicyParamCatalogEntry(const ID& policy_param_id,
                                                ErrorContext* ctx = nullptr) -> Status;

        auto upsertShardKeyCatalogEntry(const ShardKeyCatalogInfo& info,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto getShardKeyCatalogEntry(const ID& shard_key_id,
                                     ShardKeyCatalogInfo& info_out,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto listShardKeyCatalogEntries(const ID& table_id,
                                        std::vector<ShardKeyCatalogInfo>& rows_out,
                                        ErrorContext* ctx = nullptr) -> Status;
        auto deleteShardKeyCatalogEntry(const ID& shard_key_id,
                                        ErrorContext* ctx = nullptr) -> Status;

        auto upsertShardCatalogEntry(const ShardCatalogInfo& info,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto getShardCatalogEntry(const ID& shard_id,
                                  ShardCatalogInfo& info_out,
                                  ErrorContext* ctx = nullptr) -> Status;
        auto listShardCatalogEntries(const ID& cluster_id,
                                     std::vector<ShardCatalogInfo>& rows_out,
                                     ErrorContext* ctx = nullptr) -> Status;
        auto deleteShardCatalogEntry(const ID& shard_id,
                                     ErrorContext* ctx = nullptr) -> Status;

        auto upsertShardScopeCatalogEntry(const ShardScopeCatalogInfo& info,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getShardScopeCatalogEntry(const ID& scope_id,
                                       ShardScopeCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listShardScopeCatalogEntries(const ID& shard_id,
                                          std::vector<ShardScopeCatalogInfo>& rows_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteShardScopeCatalogEntry(const ID& scope_id,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertShardRangeCatalogEntry(const ShardRangeCatalogInfo& info,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto getShardRangeCatalogEntry(const ID& range_id,
                                       ShardRangeCatalogInfo& info_out,
                                       ErrorContext* ctx = nullptr) -> Status;
        auto listShardRangeCatalogEntries(const ID& shard_id,
                                          std::vector<ShardRangeCatalogInfo>& rows_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto deleteShardRangeCatalogEntry(const ID& range_id,
                                          ErrorContext* ctx = nullptr) -> Status;

        auto upsertShardReplicaCatalogEntry(const ShardReplicaCatalogInfo& info,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto getShardReplicaCatalogEntry(const ID& replica_id,
                                         ShardReplicaCatalogInfo& info_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto listShardReplicaCatalogEntries(const ID& shard_id,
                                            std::vector<ShardReplicaCatalogInfo>& rows_out,
                                            ErrorContext* ctx = nullptr) -> Status;
        auto deleteShardReplicaCatalogEntry(const ID& replica_id,
                                            ErrorContext* ctx = nullptr) -> Status;

        auto upsertShardMigrationCatalogEntry(const ShardMigrationCatalogInfo& info,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getShardMigrationCatalogEntry(const ID& migration_id,
                                           ShardMigrationCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listShardMigrationCatalogEntries(const ID& shard_id,
                                              std::vector<ShardMigrationCatalogInfo>& rows_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto deleteShardMigrationCatalogEntry(const ID& migration_id,
                                              ErrorContext* ctx = nullptr) -> Status;

        auto upsertShardZoneCatalogEntry(const ShardZoneCatalogInfo& info,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto getShardZoneCatalogEntry(const ID& zone_id,
                                      ShardZoneCatalogInfo& info_out,
                                      ErrorContext* ctx = nullptr) -> Status;
        auto listShardZoneCatalogEntries(std::vector<ShardZoneCatalogInfo>& rows_out,
                                         ErrorContext* ctx = nullptr) -> Status;
        auto deleteShardZoneCatalogEntry(const ID& zone_id,
                                         ErrorContext* ctx = nullptr) -> Status;

        auto upsertShardZoneRangeCatalogEntry(const ShardZoneRangeCatalogInfo& info,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto getShardZoneRangeCatalogEntry(const ID& zone_range_id,
                                           ShardZoneRangeCatalogInfo& info_out,
                                           ErrorContext* ctx = nullptr) -> Status;
        auto listShardZoneRangeCatalogEntries(const ID& zone_id,
                                              std::vector<ShardZoneRangeCatalogInfo>& rows_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto deleteShardZoneRangeCatalogEntry(const ID& zone_range_id,
                                              ErrorContext* ctx = nullptr) -> Status;

        auto upsertEncryptionProfileCatalogEntry(const EncryptionProfileCatalogInfo& info,
                                                 ErrorContext* ctx = nullptr) -> Status;
        auto getEncryptionProfileCatalogEntry(const ID& profile_id,
                                              EncryptionProfileCatalogInfo& info_out,
                                              ErrorContext* ctx = nullptr) -> Status;
        auto listEncryptionProfileCatalogEntries(
            std::vector<EncryptionProfileCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteEncryptionProfileCatalogEntry(const ID& profile_id,
                                                 ErrorContext* ctx = nullptr) -> Status;

        auto upsertEncryptionKeyCatalogEntry(const EncryptionKeyCatalogInfo& info,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto getEncryptionKeyCatalogEntry(const ID& key_id,
                                          EncryptionKeyCatalogInfo& info_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto listEncryptionKeyCatalogEntries(const ID& profile_id,
                                             std::vector<EncryptionKeyCatalogInfo>& rows_out,
                                             ErrorContext* ctx = nullptr) -> Status;
        auto deleteEncryptionKeyCatalogEntry(const ID& key_id,
                                             ErrorContext* ctx = nullptr) -> Status;

        auto upsertEncryptionKeyShardCatalogEntry(const EncryptionKeyShardCatalogInfo& info,
                                                  ErrorContext* ctx = nullptr) -> Status;
        auto getEncryptionKeyShardCatalogEntry(const ID& shard_id,
                                               EncryptionKeyShardCatalogInfo& info_out,
                                               ErrorContext* ctx = nullptr) -> Status;
        auto listEncryptionKeyShardCatalogEntries(
            const ID& key_id,
            std::vector<EncryptionKeyShardCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteEncryptionKeyShardCatalogEntry(const ID& shard_id,
                                                  ErrorContext* ctx = nullptr) -> Status;

        auto upsertEncryptionBootstrapInfoCatalogEntry(
            const EncryptionBootstrapInfoCatalogInfo& info,
            ErrorContext* ctx = nullptr) -> Status;
        auto getEncryptionBootstrapInfoCatalogEntry(
            const ID& database_id,
            EncryptionBootstrapInfoCatalogInfo& info_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto listEncryptionBootstrapInfoCatalogEntries(
            std::vector<EncryptionBootstrapInfoCatalogInfo>& rows_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto deleteEncryptionBootstrapInfoCatalogEntry(const ID& database_id,
                                                       ErrorContext* ctx = nullptr) -> Status;

        auto evaluateCryptoBaselinePolicy(const CryptoBaselineEvaluationRequest& request,
                                          CryptoBaselineEvaluationDecision& decision_out,
                                          ErrorContext* ctx = nullptr) -> Status;
        auto evaluateCryptoBaselineConformanceGate(const CryptoBaselineEvaluationRequest& request,
                                                   CryptoBaselineEvaluationDecision& decision_out,
                                                   ErrorContext* ctx = nullptr) -> Status;
        auto transitionEncryptionKeyLifecycle(
            const EncryptionKeyLifecycleTransitionRequest& request,
            EncryptionKeyLifecycleTransitionDecision& decision_out,
            ErrorContext* ctx = nullptr) -> Status;
        auto evaluateChannelSecurityPolicy(const ChannelSecurityEvaluationRequest& request,
                                           ChannelSecurityEvaluationDecision& decision_out,
                                           ErrorContext* ctx = nullptr) -> Status;

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
        // Returns mapping metadata with write-only credential semantics (credentials are redacted).
        auto getUserMapping(const ID& user_id, const ID& foreign_server_id,
                            UserMappingInfo& mapping_out, ErrorContext* ctx = nullptr) -> Status;
        // Runtime-only accessor for resolved credentials used by remote dispatch.
        auto getUserMappingForRuntime(const ID& user_id, const ID& foreign_server_id,
                                      UserMappingInfo& mapping_out, ErrorContext* ctx = nullptr)
            -> Status;
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
        auto updateUserMetadata(const ID& user_id, const std::string& user_metadata,
                               ErrorContext* ctx = nullptr) -> Status;
        auto renameUser(const ID& user_id, const std::string& new_username,
                        ErrorContext* ctx = nullptr) -> Status;

        auto deleteUser(const ID& user_id, bool cascade = false, ErrorContext* ctx = nullptr) -> Status;

        auto listUsers(std::vector<UserInfo>& users_out,
                      ErrorContext* ctx = nullptr) -> Status;

        auto getBootstrapState(BootstrapState& state_out,
                               ErrorContext* ctx = nullptr) -> Status;

        auto transitionBootstrapState(BootstrapState expected_state,
                                      BootstrapState new_state,
                                      ErrorContext* ctx = nullptr) -> Status;

        auto claimBootstrapWindow(ErrorContext* ctx = nullptr) -> Status;

        auto releaseBootstrapWindow(ErrorContext* ctx = nullptr) -> Status;

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
        auto revokeAuthKeysByIssuer(const std::string& issuer,
                                    uint32_t& revoked_count_out,
                                    ErrorContext* ctx = nullptr) -> Status;
        auto revokeAuthKeysByScope(AuthKeyScope scope,
                                   uint32_t& revoked_count_out,
                                   ErrorContext* ctx = nullptr) -> Status;
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

        struct SessionEpochValidation
        {
            bool valid = true;
            bool requires_replan = false;
            std::string reason_code;
            uint64_t pinned_cluster_config_epoch = 0;
            uint64_t pinned_schema_epoch = 0;
            uint64_t pinned_security_epoch = 0;
        };

        auto setSessionEpochPins(const ID& session_id,
                                 uint64_t cluster_config_epoch,
                                 uint64_t schema_epoch,
                                 uint64_t security_epoch,
                                 ErrorContext* ctx = nullptr) -> Status;

        auto validateSessionEpochPins(const ID& session_id,
                                      uint64_t cluster_config_epoch,
                                      uint64_t schema_epoch,
                                      uint64_t security_epoch,
                                      bool reject_on_mismatch,
                                      SessionEpochValidation& validation_out,
                                      ErrorContext* ctx = nullptr) -> Status;

        // Runtime monitoring
        struct TransactionHistoryEntry
        {
            uint32_t thread_id = 0;
            uint64_t event_id = 0;
            uint64_t end_event_id = 0;
            uint64_t trx_id = 0;
            uint64_t start_oit = 0;
            uint64_t end_oit = 0;
            uint64_t start_oat = 0;
            uint64_t end_oat = 0;
            uint64_t start_ost = 0;
            uint64_t end_ost = 0;
            uint64_t timer_start = 0;
            uint64_t timer_end = 0;
            uint64_t timer_wait = 0;
            uint64_t restart_count = 0;
            bool read_only = false;
            uint8_t isolation_level = 0;
            bool autocommit = false;
            bool committed = false;
            bool has_publication_fence_us = false;
            uint64_t publication_fence_us = 0;
            std::string limbo_state;
        };

        struct WaitHistoryEntry
        {
            uint32_t thread_id = 0;
            uint32_t blocker_thread_id = 0;
            uint64_t event_id = 0;
            uint64_t timer_start = 0;
            uint64_t timer_end = 0;
            uint64_t timer_wait = 0;
            uint64_t object_instance_begin = 0;
            bool has_blocker_txid = false;
            uint64_t blocker_txid = 0;
            bool has_victim_txid = false;
            uint64_t victim_txid = 0;
            std::string resource_class;
            std::string resource_id;
            uint8_t requested_mode = 0;
            uint8_t blocker_mode = 0;
            std::string outcome_code;
            std::string victim_reason_code;
            std::string blocker_identity;
            std::string victim_identity;
            bool retry_eligible = false;
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
        auto verifyAuditLogChain(AuditIntegrityResult& result_out,
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

        auto updatePreparedTransaction(const PreparedTransactionInfo& info,
                                       ErrorContext* ctx = nullptr) -> Status;

        auto createPreparedTransactionLocks(
            const ID& prepared_id,
            const std::vector<PreparedTransactionLockInfo>& locks,
            ErrorContext* ctx = nullptr) -> Status;

        auto listPreparedTransactionLocks(
            const ID& prepared_id,
            std::vector<PreparedTransactionLockInfo>& locks_out,
            ErrorContext* ctx = nullptr) -> Status;

        auto deletePreparedTransactionLocks(const ID& prepared_id,
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
            ID timezone_uuid{};
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
            ID charset_uuid{};
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
            ID charset_uuid{};
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
        auto dropTrigger(const ID &trigger_id, ErrorContext *ctx = nullptr) -> Status;
        
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
        auto enableTrigger(const ID &trigger_id, bool enable,
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
            std::string source_dialect;     // Canonical dialect/profile id for the original SQL text
            std::vector<uint8_t> native_compiled_code;  // Optional SBLR->native artifact bytes
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
            std::string source_dialect;     // Canonical dialect/profile id for the original SQL text
            std::vector<uint8_t> native_compiled_code;  // Optional SBLR->native artifact bytes
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
        auto loadStringFromToast(const ID &oid, uint64_t xmin,
                                std::string& str_out, ErrorContext* ctx = nullptr) -> Status;

        // OPT-1/OPT-2: Public TOAST storage for statistics data (MCVs, histograms)
        // Store a string in TOAST and return its OID
        // @param str String to store
        // @param xmin Transaction ID (use 0 for catalog operations)
        // @param oid_out Output OID for the stored value
        // @param ctx Error context
        // @return Status::OK on success
        auto storeStringInToast(const std::string& str, uint64_t xmin,
                               ID& oid_out, ErrorContext* ctx = nullptr) -> Status;
        auto deleteToastValue(const ID& oid, uint64_t xmax,
                             ErrorContext* ctx = nullptr) -> Status;

        // Initialize policy TOAST storage (must be called after StorageEngine is ready)
        auto initializePolicyToastIfNeeded(ErrorContext* ctx = nullptr) -> Status;

        // Plan 03B: Domains catalog page
        auto ensureDomainsTablePage(ErrorContext* ctx = nullptr) -> Status;
        auto databaseTablePage() const -> uint32_t
        {
            return database_table_page_;
        }
        auto objectTablePage() const -> uint32_t
        {
            return object_table_page_;
        }
        auto objectNameTablePage() const -> uint32_t
        {
            return object_name_table_page_;
        }
        auto domainsTablePage() const -> uint32_t
        {
            return domains_table_page_;
        }
        auto typeTablePage() const -> uint32_t
        {
            return type_table_page_;
        }
        auto typeModifiersTablePage() const -> uint32_t
        {
            return type_modifiers_table_page_;
        }
        auto typeIoTablePage() const -> uint32_t
        {
            return type_io_table_page_;
        }
        auto typeCastsTablePage() const -> uint32_t
        {
            return type_casts_table_page_;
        }
        auto typeTransformsTablePage() const -> uint32_t
        {
            return type_transforms_table_page_;
        }
        auto encodingConversionsTablePage() const -> uint32_t
        {
            return encoding_conversions_table_page_;
        }
        auto domainParamKeysTablePage() const -> uint32_t
        {
            return domain_param_keys_table_page_;
        }
        auto domainParametersTablePage() const -> uint32_t
        {
            return domain_parameters_table_page_;
        }
        auto domainConstraintsTablePage() const -> uint32_t
        {
            return domain_constraints_table_page_;
        }
        auto domainSecurityTablePage() const -> uint32_t
        {
            return domain_security_table_page_;
        }
        auto domainValidationTablePage() const -> uint32_t
        {
            return domain_validation_table_page_;
        }
        auto domainIntegrityTablePage() const -> uint32_t
        {
            return domain_integrity_table_page_;
        }
        auto charsetAliasesTablePage() const -> uint32_t
        {
            return charset_aliases_table_page_;
        }
        auto collationTailoringTablePage() const -> uint32_t
        {
            return collation_tailoring_table_page_;
        }
        auto resourceBundlesTablePage() const -> uint32_t
        {
            return resource_bundles_table_page_;
        }
        auto resourceArtifactsTablePage() const -> uint32_t
        {
            return resource_artifacts_table_page_;
        }
        auto timezoneTransitionsTablePage() const -> uint32_t
        {
            return timezone_transitions_table_page_;
        }
        auto timezoneLeapSecondsTablePage() const -> uint32_t
        {
            return timezone_leap_seconds_table_page_;
        }
        auto reservedWordsTablePage() const -> uint32_t
        {
            return reserved_words_table_page_;
        }
        auto emulationProfileTablePage() const -> uint32_t
        {
            return emulation_profile_table_page_;
        }
        auto parserProfilesTablePage() const -> uint32_t
        {
            return parser_profiles_table_page_;
        }
        auto parserCapabilityEntriesTablePage() const -> uint32_t
        {
            return parser_capability_entries_table_page_;
        }
        auto parserTransformEntriesTablePage() const -> uint32_t
        {
            return parser_transform_entries_table_page_;
        }
        auto parserErrorMapEntriesTablePage() const -> uint32_t
        {
            return parser_error_map_entries_table_page_;
        }
        auto parserFeaturePrecedenceTablePage() const -> uint32_t
        {
            return parser_feature_precedence_table_page_;
        }
        auto partitionedTablesTablePage() const -> uint32_t
        {
            return partitioned_tables_table_page_;
        }
        auto partitionsTablePage() const -> uint32_t
        {
            return partitions_table_page_;
        }
        auto tableInheritanceTablePage() const -> uint32_t
        {
            return table_inheritance_table_page_;
        }
        auto languagesTablePage() const -> uint32_t
        {
            return languages_table_page_;
        }
        auto eventsTablePage() const -> uint32_t
        {
            return events_table_page_;
        }
        auto packageMembersTablePage() const -> uint32_t
        {
            return package_members_table_page_;
        }
        auto indexColumnsTablePage() const -> uint32_t
        {
            return index_columns_table_page_;
        }
        auto indexOpclassTablePage() const -> uint32_t
        {
            return index_opclass_table_page_;
        }
        auto indexOpclassFunctionTablePage() const -> uint32_t
        {
            return index_opclass_fn_table_page_;
        }
        auto indexOptionsTablePage() const -> uint32_t
        {
            return index_options_table_page_;
        }
        auto indexAccessMethodsTablePage() const -> uint32_t
        {
            return index_access_methods_table_page_;
        }
        auto indexMaintenanceTablePage() const -> uint32_t
        {
            return index_maintenance_table_page_;
        }
        auto indexMaintenanceDeltasTablePage() const -> uint32_t
        {
            return index_maintenance_deltas_table_page_;
        }
        auto indexBuildDeltasTablePage() const -> uint32_t
        {
            return index_build_deltas_table_page_;
        }
        auto indexPageDeltasTablePage() const -> uint32_t
        {
            return index_page_deltas_table_page_;
        }
        auto indexStatsTablePage() const -> uint32_t
        {
            return index_stats_table_page_;
        }
        auto indexUsageTablePage() const -> uint32_t
        {
            return index_usage_table_page_;
        }
        auto indexContentionTablePage() const -> uint32_t
        {
            return index_contention_table_page_;
        }
        auto indexStorageTablePage() const -> uint32_t
        {
            return index_storage_table_page_;
        }
        auto indexHealthTablePage() const -> uint32_t
        {
            return index_health_table_page_;
        }

        auto filespaceStatsTablePage() const -> uint32_t
        {
            return filespace_stats_table_page_;
        }

        auto lobTablePage() const -> uint32_t
        {
            return lob_table_page_;
        }

        auto lobPageTablePage() const -> uint32_t
        {
            return lob_page_table_page_;
        }

        auto backupHistoryTablePage() const -> uint32_t
        {
            return backup_history_table_page_;
        }

        auto connectionTablePage() const -> uint32_t
        {
            return connection_table_page_;
        }

        auto transactionTablePage() const -> uint32_t
        {
            return transaction_table_page_;
        }

        auto configKeyTablePage() const -> uint32_t
        {
            return config_key_table_page_;
        }
        auto configValueTablePage() const -> uint32_t
        {
            return config_value_table_page_;
        }
        auto configChangeLogTablePage() const -> uint32_t
        {
            return config_change_log_table_page_;
        }
        auto listenerProfileTablePage() const -> uint32_t
        {
            return listener_profile_table_page_;
        }
        auto listenerBindingTablePage() const -> uint32_t
        {
            return listener_binding_table_page_;
        }
        auto listenerEmulationBindingTablePage() const -> uint32_t
        {
            return listener_emulation_binding_table_page_;
        }
        auto parserPoolPolicyTablePage() const -> uint32_t
        {
            return parser_pool_policy_table_page_;
        }
        auto listenerRuntimeTargetTablePage() const -> uint32_t
        {
            return listener_runtime_target_table_page_;
        }
        auto listenerGenerationRecordTablePage() const -> uint32_t
        {
            return listener_generation_record_table_page_;
        }
        auto schemaChangePlanTablePage() const -> uint32_t
        {
            return schema_change_plan_table_page_;
        }
        auto schemaChangeEventTablePage() const -> uint32_t
        {
            return schema_change_event_table_page_;
        }
        auto schemaChangeBackfillProgressTablePage() const -> uint32_t
        {
            return schema_change_backfill_progress_table_page_;
        }
        auto schemaChangeCutoverGuardTablePage() const -> uint32_t
        {
            return schema_change_cutover_guard_table_page_;
        }
        auto indexBuildPlanTablePage() const -> uint32_t
        {
            return index_build_plan_table_page_;
        }
        auto indexBuildEventTablePage() const -> uint32_t
        {
            return index_build_event_table_page_;
        }
        auto indexBuildProgressTablePage() const -> uint32_t
        {
            return index_build_progress_table_page_;
        }
        auto indexBuildCutoverGuardTablePage() const -> uint32_t
        {
            return index_build_cutover_guard_table_page_;
        }
        auto bulkLoadPlanTablePage() const -> uint32_t
        {
            return bulk_load_plan_table_page_;
        }
        auto bulkLoadEventTablePage() const -> uint32_t
        {
            return bulk_load_event_table_page_;
        }
        auto bulkLoadProgressTablePage() const -> uint32_t
        {
            return bulk_load_progress_table_page_;
        }
        auto bulkLoadCutoverGuardTablePage() const -> uint32_t
        {
            return bulk_load_cutover_guard_table_page_;
        }
        auto memoryGrantFeedbackTablePage() const -> uint32_t
        {
            return memory_grant_feedback_table_page_;
        }
        auto authMappingTablePage() const -> uint32_t
        {
            return auth_mapping_table_page_;
        }
        auto authProviderTablePage() const -> uint32_t
        {
            return auth_provider_table_page_;
        }
        auto authPolicyTablePage() const -> uint32_t
        {
            return auth_policy_table_page_;
        }
        auto mfaPolicyTablePage() const -> uint32_t
        {
            return mfa_policy_table_page_;
        }
        auto mfaEnrollmentTablePage() const -> uint32_t
        {
            return mfa_enrollment_table_page_;
        }
        auto mfaRecoveryCodeTablePage() const -> uint32_t
        {
            return mfa_recovery_code_table_page_;
        }
        auto authAttemptLogTablePage() const -> uint32_t
        {
            return auth_attempt_log_table_page_;
        }
        auto connectionRuleTablePage() const -> uint32_t
        {
            return connection_rule_table_page_;
        }
        auto connectionRuleEpochTablePage() const -> uint32_t
        {
            return connection_rule_epoch_table_page_;
        }
        auto roleSettingTablePage() const -> uint32_t
        {
            return role_setting_table_page_;
        }
        auto securityLabelTablePage() const -> uint32_t
        {
            return security_label_table_page_;
        }
        auto securityClassTablePage() const -> uint32_t
        {
            return security_class_table_page_;
        }
        auto certRegistryTablePage() const -> uint32_t
        {
            return cert_registry_table_page_;
        }
        auto privateKeyStoreTablePage() const -> uint32_t
        {
            return private_key_store_table_page_;
        }
        auto trustAnchorTablePage() const -> uint32_t
        {
            return trust_anchor_table_page_;
        }
        auto channelCertBindingTablePage() const -> uint32_t
        {
            return channel_cert_binding_table_page_;
        }
        auto certRevocationTablePage() const -> uint32_t
        {
            return cert_revocation_table_page_;
        }
        auto pkiDistributionStateTablePage() const -> uint32_t
        {
            return pki_distribution_state_table_page_;
        }
        auto trustAnchorRolloverTablePage() const -> uint32_t
        {
            return trust_anchor_rollover_table_page_;
        }
        auto encryptionProfileTablePage() const -> uint32_t
        {
            return encryption_profile_table_page_;
        }
        auto encryptionKeyTablePage() const -> uint32_t
        {
            return encryption_key_table_page_;
        }
        auto encryptionKeyShardTablePage() const -> uint32_t
        {
            return encryption_key_shard_table_page_;
        }
        auto encryptionBootstrapInfoTablePage() const -> uint32_t
        {
            return encryption_bootstrap_info_table_page_;
        }
        auto nodeTablePage() const -> uint32_t
        {
            return node_table_page_;
        }
        auto nodeRoleBindingTablePage() const -> uint32_t
        {
            return node_role_binding_table_page_;
        }
        auto nodeServiceTablePage() const -> uint32_t
        {
            return node_service_table_page_;
        }
        auto nodeCapabilityTablePage() const -> uint32_t
        {
            return node_capability_table_page_;
        }
        auto clockPolicyTablePage() const -> uint32_t
        {
            return clock_policy_table_page_;
        }
        auto clockSourceTablePage() const -> uint32_t
        {
            return clock_source_table_page_;
        }
        auto nodeClockStateTablePage() const -> uint32_t
        {
            return node_clock_state_table_page_;
        }
        auto clockViolationEventTablePage() const -> uint32_t
        {
            return clock_violation_event_table_page_;
        }
        auto clusterTablePage() const -> uint32_t
        {
            return cluster_table_page_;
        }
        auto shardPolicyTablePage() const -> uint32_t
        {
            return shard_policy_table_page_;
        }
        auto shardPolicyParamTablePage() const -> uint32_t
        {
            return shard_policy_param_table_page_;
        }
        auto shardKeyTablePage() const -> uint32_t
        {
            return shard_key_table_page_;
        }
        auto shardTablePage() const -> uint32_t
        {
            return shard_table_page_;
        }
        auto shardScopeTablePage() const -> uint32_t
        {
            return shard_scope_table_page_;
        }
        auto shardRangeTablePage() const -> uint32_t
        {
            return shard_range_table_page_;
        }
        auto shardReplicaTablePage() const -> uint32_t
        {
            return shard_replica_table_page_;
        }
        auto shardMigrationTablePage() const -> uint32_t
        {
            return shard_migration_table_page_;
        }
        auto shardZoneTablePage() const -> uint32_t
        {
            return shard_zone_table_page_;
        }
        auto shardZoneRangeTablePage() const -> uint32_t
        {
            return shard_zone_range_table_page_;
        }
        auto workloadClassTablePage() const -> uint32_t
        {
            return workload_class_table_page_;
        }
        auto workloadRouteTablePage() const -> uint32_t
        {
            return workload_route_table_page_;
        }
        auto admissionPolicyTablePage() const -> uint32_t
        {
            return admission_policy_table_page_;
        }
        auto admissionBindingTablePage() const -> uint32_t
        {
            return admission_binding_table_page_;
        }
        auto sloProfileTablePage() const -> uint32_t
        {
            return slo_profile_table_page_;
        }
        auto sloBindingTablePage() const -> uint32_t
        {
            return slo_binding_table_page_;
        }
        auto sloWindowTablePage() const -> uint32_t
        {
            return slo_window_table_page_;
        }
        auto sloBurnEventTablePage() const -> uint32_t
        {
            return slo_burn_event_table_page_;
        }
        auto autoscalePolicyTablePage() const -> uint32_t
        {
            return autoscale_policy_table_page_;
        }
        auto autoscaleActionTablePage() const -> uint32_t
        {
            return autoscale_action_table_page_;
        }
        auto admissionTuningEventTablePage() const -> uint32_t
        {
            return admission_tuning_event_table_page_;
        }
        auto clusterPolicyTablePage() const -> uint32_t
        {
            return cluster_policy_table_page_;
        }
        auto failureDetectorTablePage() const -> uint32_t
        {
            return failure_detector_table_page_;
        }
        auto alertRuleTablePage() const -> uint32_t
        {
            return alert_rule_table_page_;
        }
        auto alertTargetTablePage() const -> uint32_t
        {
            return alert_target_table_page_;
        }
        auto alertRouteTablePage() const -> uint32_t
        {
            return alert_route_table_page_;
        }
        auto alertEventTablePage() const -> uint32_t
        {
            return alert_event_table_page_;
        }
        auto alertAckTablePage() const -> uint32_t
        {
            return alert_ack_table_page_;
        }
        auto alertSilenceTablePage() const -> uint32_t
        {
            return alert_silence_table_page_;
        }
        auto networkPartitionEventTablePage() const -> uint32_t
        {
            return network_partition_event_table_page_;
        }
        auto networkPartitionMemberTablePage() const -> uint32_t
        {
            return network_partition_member_table_page_;
        }
        auto healingPolicyTablePage() const -> uint32_t
        {
            return healing_policy_table_page_;
        }
        auto healingActionTablePage() const -> uint32_t
        {
            return healing_action_table_page_;
        }
        auto healingActionParamTablePage() const -> uint32_t
        {
            return healing_action_param_table_page_;
        }
        auto healingRunTablePage() const -> uint32_t
        {
            return healing_run_table_page_;
        }
        auto healingStepTablePage() const -> uint32_t
        {
            return healing_step_table_page_;
        }
        auto jobTypeTablePage() const -> uint32_t
        {
            return job_type_table_page_;
        }
        auto jobTypeParamTablePage() const -> uint32_t
        {
            return job_type_param_table_page_;
        }
        auto jobParamTablePage() const -> uint32_t
        {
            return job_param_table_page_;
        }
        auto jobScheduleTablePage() const -> uint32_t
        {
            return job_schedule_table_page_;
        }
        auto jobTypePolicyTablePage() const -> uint32_t
        {
            return job_type_policy_table_page_;
        }
        auto remoteConnectorTablePage() const -> uint32_t
        {
            return remote_connector_table_page_;
        }
        auto remoteConnectorCapabilityTablePage() const -> uint32_t
        {
            return remote_connector_capability_table_page_;
        }
        auto remoteMetadataSnapshotTablePage() const -> uint32_t
        {
            return remote_metadata_snapshot_table_page_;
        }
        auto remoteMetadataObjectTablePage() const -> uint32_t
        {
            return remote_metadata_object_table_page_;
        }
        auto remoteMetadataColumnTablePage() const -> uint32_t
        {
            return remote_metadata_column_table_page_;
        }
        auto remoteSchemaMappingTablePage() const -> uint32_t
        {
            return remote_schema_mapping_table_page_;
        }
        auto remotePassthroughPolicyTablePage() const -> uint32_t
        {
            return remote_passthrough_policy_table_page_;
        }
        auto remotePreparedStatementTablePage() const -> uint32_t
        {
            return remote_prepared_statement_table_page_;
        }
        auto remoteTxnBindingTablePage() const -> uint32_t
        {
            return remote_txn_binding_table_page_;
        }
        auto remoteExecutionAuditTablePage() const -> uint32_t
        {
            return remote_execution_audit_table_page_;
        }
        auto remoteErrorTablePage() const -> uint32_t
        {
            return remote_error_table_page_;
        }
        auto extensionTablePage() const -> uint32_t
        {
            return extensions_table_page_;
        }
        auto publicationTablePage() const -> uint32_t
        {
            return publication_table_page_;
        }
        auto publicationTableLinkTablePage() const -> uint32_t
        {
            return publication_table_link_table_page_;
        }
        auto publicationSchemaTablePage() const -> uint32_t
        {
            return publication_schema_table_page_;
        }
        auto subscriptionTablePage() const -> uint32_t
        {
            return subscription_table_page_;
        }
        auto subscriptionTableLinkTablePage() const -> uint32_t
        {
            return subscription_table_link_table_page_;
        }
        auto clusterFabricLinkTablePage() const -> uint32_t
        {
            return cluster_fabric_link_table_page_;
        }
        auto clusterFabricSessionTablePage() const -> uint32_t
        {
            return cluster_fabric_session_table_page_;
        }
        auto clusterFabricTxnTablePage() const -> uint32_t
        {
            return cluster_fabric_txn_table_page_;
        }
        auto clusterFabricTaskTablePage() const -> uint32_t
        {
            return cluster_fabric_task_table_page_;
        }
        auto clusterFabricTaskChunkTablePage() const -> uint32_t
        {
            return cluster_fabric_task_chunk_table_page_;
        }
        auto clusterFabricEventTablePage() const -> uint32_t
        {
            return cluster_fabric_event_table_page_;
        }
        auto clusterFabricErrorTablePage() const -> uint32_t
        {
            return cluster_fabric_error_table_page_;
        }
        auto olapWatermarkTablePage() const -> uint32_t
        {
            return olap_watermark_table_page_;
        }
        auto olapPartitionTablePage() const -> uint32_t
        {
            return olap_partition_table_page_;
        }
        auto olapSegmentTablePage() const -> uint32_t
        {
            return olap_segment_table_page_;
        }
        auto olapIngestLogTablePage() const -> uint32_t
        {
            return olap_ingest_log_table_page_;
        }
        auto cubeTablePage() const -> uint32_t
        {
            return cube_table_page_;
        }
        auto cubeDimensionTablePage() const -> uint32_t
        {
            return cube_dimension_table_page_;
        }
        auto cubeLevelTablePage() const -> uint32_t
        {
            return cube_level_table_page_;
        }
        auto cubeHierarchyTablePage() const -> uint32_t
        {
            return cube_hierarchy_table_page_;
        }
        auto cubeHierarchyLevelTablePage() const -> uint32_t
        {
            return cube_hierarchy_level_table_page_;
        }
        auto cubeMeasureTablePage() const -> uint32_t
        {
            return cube_measure_table_page_;
        }
        auto cubeMaterializationTablePage() const -> uint32_t
        {
            return cube_materialization_table_page_;
        }
        auto cubeRefreshPolicyTablePage() const -> uint32_t
        {
            return cube_refresh_policy_table_page_;
        }
        auto cubeJobTablePage() const -> uint32_t
        {
            return cube_job_table_page_;
        }
        auto cubeJobStepTablePage() const -> uint32_t
        {
            return cube_job_step_table_page_;
        }
        auto cubeStatsTablePage() const -> uint32_t
        {
            return cube_stats_table_page_;
        }
        auto tsParserTablePage() const -> uint32_t
        {
            return ts_parser_table_page_;
        }
        auto tsTemplateTablePage() const -> uint32_t
        {
            return ts_template_table_page_;
        }
        auto tsDictionaryTablePage() const -> uint32_t
        {
            return ts_dictionary_table_page_;
        }
        auto tsConfigTablePage() const -> uint32_t
        {
            return ts_config_table_page_;
        }
        auto tsConfigMapTablePage() const -> uint32_t
        {
            return ts_config_map_table_page_;
        }
        auto blobFilterTablePage() const -> uint32_t
        {
            return blob_filter_table_page_;
        }
        auto triggerMessageTablePage() const -> uint32_t
        {
            return trigger_message_table_page_;
        }
        auto columnDropHistoryTablePage() const -> uint32_t
        {
            return column_drop_history_table_page_;
        }
        auto sblrModuleTablePage() const -> uint32_t
        {
            return sblr_module_table_page_;
        }
        auto sblrPlanTablePage() const -> uint32_t
        {
            return sblr_plan_table_page_;
        }
        auto sblrPlanDependencyTablePage() const -> uint32_t
        {
            return sblr_plan_dependency_table_page_;
        }
        auto sblrStatementNormTablePage() const -> uint32_t
        {
            return sblr_statement_norm_table_page_;
        }
        auto sblrArtifactTablePage() const -> uint32_t
        {
            return sblr_artifact_table_page_;
        }
        auto sblrArtifactStatsTablePage() const -> uint32_t
        {
            return sblr_artifact_stats_table_page_;
        }
        auto sblrCompilerTargetTablePage() const -> uint32_t
        {
            return sblr_compiler_target_table_page_;
        }
        auto sblrCompileQueueTablePage() const -> uint32_t
        {
            return sblr_compile_queue_table_page_;
        }
        auto replicationChannelTablePage() const -> uint32_t
        {
            return replication_channel_table_page_;
        }
        auto replicationChannelMemberTablePage() const -> uint32_t
        {
            return replication_channel_member_table_page_;
        }
        auto replicationOriginTablePage() const -> uint32_t
        {
            return replication_origin_table_page_;
        }
        auto replicationCursorTablePage() const -> uint32_t
        {
            return replication_cursor_table_page_;
        }
        auto replicationOriginProgressTablePage() const -> uint32_t
        {
            return replication_origin_progress_table_page_;
        }
        auto replicationTxnBatchTablePage() const -> uint32_t
        {
            return replication_txn_batch_table_page_;
        }
        auto replicationApplyLogTablePage() const -> uint32_t
        {
            return replication_apply_log_table_page_;
        }
        auto replicationRetryQueueTablePage() const -> uint32_t
        {
            return replication_retry_queue_table_page_;
        }
        auto replicationConflictTablePage() const -> uint32_t
        {
            return replication_conflict_table_page_;
        }
        auto replicationSplitBrainEventTablePage() const -> uint32_t
        {
            return replication_split_brain_event_table_page_;
        }
        auto replicationErrorTablePage() const -> uint32_t
        {
            return replication_error_table_page_;
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
        auto validateColumnDomains(const std::vector<ColumnInfo>& columns,
                                   ErrorContext* ctx) -> Status;
        auto applySystemDomainDefaults(const ID& schema_id,
                                       const std::string& table_name,
                                       std::vector<ColumnInfo>& columns,
                                       ErrorContext* ctx) -> Status;
        auto enforceSystemDomainBindings(ErrorContext* ctx) -> Status;
        auto ensureHomeSearchPathCatalogTables(ErrorContext* ctx) -> Status;
        auto ensureMfaCatalogTables(ErrorContext* ctx) -> Status;
        auto resolveSessionHomeSchema(const UserInfo& user,
                                      const std::vector<ID>& effective_roles,
                                      const std::vector<ID>& effective_groups,
                                      const std::string& emulation_mode,
                                      ID& home_schema_id_out,
                                      ErrorContext* ctx) -> Status;
        auto resolveSessionSearchPath(const ID& user_id,
                                      const std::vector<ID>& effective_roles,
                                      const std::vector<ID>& effective_groups,
                                      const std::string& emulation_mode,
                                      const ID& home_schema_id,
                                      ID& profile_id_out,
                                      std::vector<ID>& schema_ids_out,
                                      std::vector<std::string>& schema_paths_out,
                                      ErrorContext* ctx) -> Status;
        auto updateColumnDomainBindings(const ID& table_id,
                                        const std::vector<ColumnInfo>& columns,
                                        ErrorContext* ctx) -> Status;
        auto isSystemSchemaId(const ID& schema_id) const -> bool;

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
        auto dropTriggerInternal(const ID& trigger_id, ErrorContext* ctx) -> Status;

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
        std::unordered_map<ID, std::string, IDHash> toast_fallback_cache_;
        std::mutex toast_fallback_mutex_;
        ID toast_fallback_next_oid_{};
        std::unordered_map<ID, std::string, IDHash> schema_epoch_manifest_cache_;
        bool schema_epoch_catalog_rebuild_required_ = false;
        std::unordered_map<ID, std::pair<std::string, std::string>, IDHash>
            forensic_snapshot_capsule_manifest_cache_;

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
        auto ensureOverlaySchemaChildUnlocked(const std::string& root_schema_name,
                                              const std::string& child_name,
                                              ID* schema_id_out,
                                              ErrorContext* ctx) -> Status;
        auto ensureOverlaySchemaChildByParentUnlocked(const ID& parent_schema_id,
                                                      const std::string& child_name,
                                                      ID* schema_id_out,
                                                      ErrorContext* ctx) -> Status;
        auto dropOverlaySchemaChildUnlocked(const std::string& root_schema_name,
                                            const std::string& child_name,
                                            ErrorContext* ctx) -> Status;
        auto resolveEmulatedEngineNameUnlocked(const ID& server_id,
                                               std::string& engine_name_out,
                                               ErrorContext* ctx) -> Status;
        auto ensureEmulatedDatabaseOverlayUnlocked(const ID& server_id,
                                                   const std::string& database_name,
                                                   ErrorContext* ctx) -> Status;
        auto dropEmulatedDatabaseOverlayUnlocked(const ID& server_id,
                                                 const std::string& database_name,
                                                 ErrorContext* ctx) -> Status;

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

        // Canonical bootstrap map places catalog root on fixed page 2.
        static constexpr uint32_t CATALOG_ROOT_PAGE = BOOTSTRAP_PAGE_CATALOG_ROOT;
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
        std::unordered_map<uint16_t, ID> tablespace_id_to_uuid_;
        std::unordered_map<ID, uint16_t, IDHash> tablespace_uuid_to_id_;
        std::unordered_map<uint16_t, ID> charset_id_to_uuid_;
        std::unordered_map<ID, uint16_t, IDHash> charset_uuid_to_id_;
        std::unordered_map<uint16_t, ID> timezone_id_to_uuid_;
        std::unordered_map<ID, uint16_t, IDHash> timezone_uuid_to_id_;

        uint16_t next_charset_id_ = 100;
        uint16_t next_timezone_id_ = 100;

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
        std::unordered_map<std::pair<ID, std::string>, ID, PairHash<ID, std::string>>
            trigger_name_to_id_;  // (table_id, normalized_name) -> trigger_id
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
        uint32_t database_table_page_ = 0;      // Canonical core catalog: database
        uint32_t object_table_page_ = 0;        // Canonical core catalog: object
        uint32_t object_name_table_page_ = 0;   // Canonical core catalog: object_name
        uint32_t schemas_table_page_ = SCHEMAS_TABLE_PAGE;
        uint32_t tables_table_page_ = TABLES_TABLE_PAGE;
        uint32_t columns_table_page_ = COLUMNS_TABLE_PAGE;
        uint32_t indexes_table_page_ = INDEXES_TABLE_PAGE;
        uint32_t index_versions_table_page_ = 0;  // Index versions/history table
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
        uint32_t type_table_page_ = 0;              // Canonical type metadata (CAT-010)
        uint32_t type_modifiers_table_page_ = 0;    // Type modifiers (CAT-010)
        uint32_t type_io_table_page_ = 0;           // Type IO handlers (CAT-010)
        uint32_t type_casts_table_page_ = 0;        // Type casts (CAT-010)
        uint32_t type_transforms_table_page_ = 0;   // Type transforms (CAT-010)
        uint32_t encoding_conversions_table_page_ = 0; // Charset conversions (CAT-010)
        uint32_t domain_param_keys_table_page_ = 0; // Domain parameter key registry (CAT-011)
        uint32_t domain_parameters_table_page_ = 0; // Domain typed parameters (CAT-011)
        uint32_t domain_constraints_table_page_ = 0; // Domain constraints (CAT-011)
        uint32_t domain_security_table_page_ = 0;   // Domain security rules (CAT-011)
        uint32_t domain_validation_table_page_ = 0; // Domain validation rules (CAT-011)
        uint32_t domain_integrity_table_page_ = 0;  // Domain integrity rules (CAT-011)
        uint32_t charset_aliases_table_page_ = 0;   // Charset alias registry (CAT-012)
        uint32_t collation_tailoring_table_page_ = 0; // Collation tailoring registry (CAT-012)
        uint32_t resource_bundles_table_page_ = 0;  // Resource bundle registry (CAT-013)
        uint32_t resource_artifacts_table_page_ = 0; // Resource artifact blobs (CAT-013)
        uint32_t timezone_transitions_table_page_ = 0; // Timezone transitions (CAT-013)
        uint32_t timezone_leap_seconds_table_page_ = 0; // Timezone leap seconds (CAT-013)
        uint32_t reserved_words_table_page_ = 0;    // Reserved words catalog (CAT-014)
        uint32_t emulation_profile_table_page_ = 0; // Emulation profile catalog (CAT-014)
        uint32_t parser_profiles_table_page_ = 0;   // Parser profile catalog (CAT-014)
        uint32_t parser_capability_entries_table_page_ = 0; // Parser capability catalog (CAT-014)
        uint32_t parser_transform_entries_table_page_ = 0; // Parser transform catalog (CAT-014)
        uint32_t parser_error_map_entries_table_page_ = 0; // Parser error map catalog (CAT-014)
        uint32_t parser_feature_precedence_table_page_ = 0; // Parser precedence catalog (CAT-014)
        uint32_t partitioned_tables_table_page_ = 0; // Partitioned table catalog (CAT-015)
        uint32_t partitions_table_page_ = 0;      // Partition catalog (CAT-015)
        uint32_t table_inheritance_table_page_ = 0; // Table inheritance catalog (CAT-015)
        uint32_t languages_table_page_ = 0;       // Language catalog (CAT-015)
        uint32_t events_table_page_ = 0;          // Event catalog (CAT-015)
        uint32_t package_members_table_page_ = 0; // Package member catalog (CAT-015)
        uint32_t index_columns_table_page_ = 0;   // Index column catalog (CAT-016)
        uint32_t index_opclass_table_page_ = 0;   // Index opclass catalog (CAT-016)
        uint32_t index_opclass_fn_table_page_ = 0; // Index opclass function catalog (CAT-016)
        uint32_t index_options_table_page_ = 0;   // Index option catalog (CAT-016)
        uint32_t index_access_methods_table_page_ = 0; // Index access method catalog (CAT-016)
        uint32_t index_maintenance_table_page_ = 0; // Index maintenance catalog (CAT-016)
        uint32_t index_maintenance_deltas_table_page_ = 0; // Index maintenance delta catalog (CAT-016)
        uint32_t index_build_deltas_table_page_ = 0; // Index build delta catalog (CAT-016)
        uint32_t index_page_deltas_table_page_ = 0; // Index page delta catalog (CAT-016)
        uint32_t index_stats_table_page_ = 0; // Index statistics catalog (CAT-017)
        uint32_t index_usage_table_page_ = 0; // Index usage telemetry catalog (CAT-017)
        uint32_t index_contention_table_page_ = 0; // Index contention telemetry catalog (CAT-017)
        uint32_t index_storage_table_page_ = 0; // Index storage telemetry catalog (CAT-017)
        uint32_t index_health_table_page_ = 0; // Index health telemetry catalog (CAT-017)
        uint32_t filespace_stats_table_page_ = 0; // Filespace usage/IO telemetry catalog (CAT-018)
        uint32_t lob_table_page_ = 0; // Large object metadata catalog (CAT-018)
        uint32_t lob_page_table_page_ = 0; // Large object page map catalog (CAT-018)
        uint32_t backup_history_table_page_ = 0; // Backup history catalog (CAT-018)
        uint32_t audit_sink_profile_table_page_ = 0; // Audit sink profile catalog (NCW-034)
        uint32_t audit_export_segment_table_page_ = 0; // Audit export segment catalog (NCW-034)
        uint32_t transaction_lineage_event_table_page_ = 0; // Retained transaction lineage catalog (NCW-040)
        uint32_t schema_epoch_table_page_ = 0; // Historical schema epoch catalog (NCW-046)
        uint32_t schema_change_plan_table_page_ = 0; // Durable schema change plan catalog (B1-02-004)
        uint32_t schema_change_event_table_page_ = 0; // Durable schema change event catalog (B1-02-004)
        uint32_t schema_change_backfill_progress_table_page_ = 0; // Durable schema change progress catalog (B1-02-004)
        uint32_t schema_change_cutover_guard_table_page_ = 0; // Durable schema change cutover guard catalog (B1-02-004)
        uint32_t index_build_plan_table_page_ = 0; // Durable index build plan catalog (B1-08-004)
        uint32_t index_build_event_table_page_ = 0; // Durable index build event catalog (B1-08-004)
        uint32_t index_build_progress_table_page_ = 0; // Durable index build progress catalog (B1-08-004)
        uint32_t index_build_cutover_guard_table_page_ = 0; // Durable index build cutover guard catalog (B1-08-004)
        uint32_t bulk_load_plan_table_page_ = 0; // Durable bulk load plan catalog (B1-08-004)
        uint32_t bulk_load_event_table_page_ = 0; // Durable bulk load event catalog (B1-08-004)
        uint32_t bulk_load_progress_table_page_ = 0; // Durable bulk load progress catalog (B1-08-004)
        uint32_t bulk_load_cutover_guard_table_page_ = 0; // Durable bulk load cutover guard catalog (B1-08-004)
        uint32_t memory_grant_feedback_table_page_ = 0; // Durable memory grant feedback catalog (B1-08-004)
        uint32_t page_audit_finding_table_page_ = 0; // Sweep page audit findings catalog (NCW-043)
        uint32_t shadow_capture_manifest_table_page_ = 0; // Sweep shadow capture manifest catalog (NCW-044)
        uint32_t forensic_snapshot_capsule_table_page_ = 0; // Retained replay snapshot capsule catalog (NCW-045)
        uint32_t connection_table_page_ = 0; // Runtime connection attribution catalog (CAT-019)
        uint32_t transaction_table_page_ = 0; // Runtime transaction attribution catalog (CAT-019)
        uint32_t checkpoint_run_table_page_ = 0; // Checkpoint run history catalog (TDRW-014)
        uint32_t recovery_run_table_page_ = 0; // Recovery run history catalog (TDRW-014)
        uint32_t sweep_cursor_state_table_page_ = 0; // Sweep cursor history catalog (TDRW-014)
        uint32_t writeback_incident_table_page_ = 0; // Writeback incident history catalog (TDRW-014)
        uint32_t recovery_incident_table_page_ = 0; // Recovery incident history catalog (TDRW-014)
        uint32_t principal_account_table_page_ = 0; // Principal account catalog (CAT-020)
        uint32_t account_credential_table_page_ = 0; // Account credential catalog (CAT-020)
        uint32_t account_profile_binding_table_page_ = 0; // Account profile binding catalog (CAT-020)
        uint32_t auth_provider_table_page_ = 0; // Auth provider catalog (EN-018)
        uint32_t auth_policy_table_page_ = 0; // Auth policy catalog (EN-018)
        uint32_t mfa_policy_table_page_ = 0; // MFA policy catalog (EN-018)
        uint32_t mfa_enrollment_table_page_ = 0; // MFA enrollment catalog (EN-018)
        uint32_t mfa_recovery_code_table_page_ = 0; // MFA recovery code catalog (EN-018)
        uint32_t auth_attempt_log_table_page_ = 0; // Auth attempt log catalog (EN-018)
        uint32_t connection_rule_table_page_ = 0; // Connection rule catalog (EN-018)
        uint32_t connection_rule_epoch_table_page_ = 0; // Connection rule epoch catalog (EN-018)
        uint32_t acl_command_catalog_table_page_ = 0; // ACL command catalog (EN-019)
        uint32_t acl_rule_table_page_ = 0; // ACL rule catalog (EN-019)
        uint32_t document_policy_table_page_ = 0; // Document ABAC policy catalog (EN-019)
        uint32_t tenant_binding_table_page_ = 0; // Tenant binding catalog (EN-019)
        uint32_t graph_privilege_table_page_ = 0; // Graph privilege catalog (EN-019)
        uint32_t token_table_page_ = 0; // Token catalog (EN-019)
        uint32_t token_scope_entry_table_page_ = 0; // Token scope entry catalog (EN-019)
        uint32_t quota_profile_table_page_ = 0; // Quota profile catalog (EN-019)
        uint32_t quota_binding_table_page_ = 0; // Quota binding catalog (EN-019)
        uint32_t settings_profile_table_page_ = 0; // Settings profile catalog (EN-019)
        uint32_t settings_binding_table_page_ = 0; // Settings binding catalog (EN-019)
        uint32_t config_key_table_page_ = 0; // Config key catalog (CFG-001)
        uint32_t config_value_table_page_ = 0; // Config value catalog (CFG-001)
        uint32_t config_change_log_table_page_ = 0; // Config change log catalog (CFG-001)
        uint32_t listener_profile_table_page_ = 0; // Listener profile catalog (CFG-002)
        uint32_t listener_binding_table_page_ = 0; // Listener binding catalog (CFG-002)
        uint32_t listener_emulation_binding_table_page_ = 0; // Listener emulation binding catalog (CFG-002)
        uint32_t parser_pool_policy_table_page_ = 0; // Parser pool policy catalog (CFG-002)
        uint32_t listener_runtime_target_table_page_ = 0; // Listener runtime target catalog (CFG-002)
        uint32_t listener_generation_record_table_page_ = 0; // Listener generation record catalog (CFG-002)
        uint32_t auth_mapping_table_page_ = 0; // Auth mapping catalog (CAT-020)
        uint32_t role_setting_table_page_ = 0; // Role setting catalog (CAT-020)
        uint32_t security_label_table_page_ = 0; // Security label catalog (CAT-020)
        uint32_t security_class_table_page_ = 0; // Security class catalog (CAT-020)
        uint32_t cert_registry_table_page_ = 0; // PKI cert registry catalog (CAT-020)
        uint32_t private_key_store_table_page_ = 0; // PKI private key store catalog (CAT-020)
        uint32_t trust_anchor_table_page_ = 0; // PKI trust anchor catalog (CAT-020)
        uint32_t channel_cert_binding_table_page_ = 0; // PKI channel cert binding catalog (CAT-020)
        uint32_t cert_revocation_table_page_ = 0; // PKI cert revocation catalog (CAT-020)
        uint32_t pki_distribution_state_table_page_ = 0; // PKI distribution state catalog (CAT-020)
        uint32_t trust_anchor_rollover_table_page_ = 0; // PKI trust anchor rollover catalog (CAT-020)
        uint32_t encryption_profile_table_page_ = 0; // Crypto profile catalog (CAT-020)
        uint32_t encryption_key_table_page_ = 0; // Crypto key catalog (CAT-020)
        uint32_t encryption_key_shard_table_page_ = 0; // Crypto key shard catalog (CAT-020)
        uint32_t encryption_bootstrap_info_table_page_ = 0; // Crypto bootstrap info catalog (CAT-020)
        uint32_t node_table_page_ = 0; // Cluster node catalog (CAT-021)
        uint32_t node_role_binding_table_page_ = 0; // Cluster node role binding catalog (CAT-021)
        uint32_t node_service_table_page_ = 0; // Cluster node service catalog (CAT-021)
        uint32_t node_capability_table_page_ = 0; // Cluster node capability catalog (CAT-021)
        uint32_t clock_policy_table_page_ = 0; // Cluster clock policy catalog (CAT-021)
        uint32_t clock_source_table_page_ = 0; // Cluster clock source catalog (CAT-021)
        uint32_t node_clock_state_table_page_ = 0; // Cluster node clock state catalog (CAT-021)
        uint32_t clock_violation_event_table_page_ = 0; // Cluster clock violation event catalog (CAT-021)
        uint32_t cluster_table_page_ = 0; // Cluster catalog (CAT-022)
        uint32_t shard_policy_table_page_ = 0; // Shard policy catalog (CAT-022)
        uint32_t shard_policy_param_table_page_ = 0; // Shard policy param catalog (CAT-022)
        uint32_t shard_key_table_page_ = 0; // Shard key catalog (CAT-022)
        uint32_t shard_table_page_ = 0; // Shard catalog (CAT-022)
        uint32_t shard_scope_table_page_ = 0; // Shard scope catalog (CAT-022)
        uint32_t shard_range_table_page_ = 0; // Shard range catalog (CAT-022)
        uint32_t shard_replica_table_page_ = 0; // Shard replica catalog (CAT-022)
        uint32_t shard_migration_table_page_ = 0; // Shard migration catalog (CAT-022)
        uint32_t shard_zone_table_page_ = 0; // Shard zone catalog (CAT-022)
        uint32_t shard_zone_range_table_page_ = 0; // Shard zone range catalog (CAT-022)
        uint32_t workload_class_table_page_ = 0; // Workload class catalog (CAT-023)
        uint32_t workload_route_table_page_ = 0; // Workload route catalog (CAT-023)
        uint32_t admission_policy_table_page_ = 0; // Admission policy catalog (CAT-023)
        uint32_t admission_binding_table_page_ = 0; // Admission binding catalog (CAT-023)
        uint32_t slo_profile_table_page_ = 0; // SLO profile catalog (CAT-023)
        uint32_t slo_binding_table_page_ = 0; // SLO binding catalog (CAT-023)
        uint32_t slo_window_table_page_ = 0; // SLO window catalog (CAT-023)
        uint32_t slo_burn_event_table_page_ = 0; // SLO burn event catalog (CAT-023)
        uint32_t autoscale_policy_table_page_ = 0; // Autoscale policy catalog (CAT-023)
        uint32_t autoscale_action_table_page_ = 0; // Autoscale action catalog (CAT-023)
        uint32_t admission_tuning_event_table_page_ = 0; // Admission tuning event catalog (CAT-023)
        uint32_t cluster_policy_table_page_ = 0; // Cluster policy catalog (CAT-024)
        uint32_t failure_detector_table_page_ = 0; // Failure detector catalog (CAT-024)
        uint32_t alert_rule_table_page_ = 0; // Alert rule catalog (CAT-024)
        uint32_t alert_target_table_page_ = 0; // Alert target catalog (CAT-024)
        uint32_t alert_route_table_page_ = 0; // Alert route catalog (CAT-024)
        uint32_t alert_event_table_page_ = 0; // Alert event catalog (CAT-024)
        uint32_t alert_ack_table_page_ = 0; // Alert ack catalog (CAT-024)
        uint32_t alert_silence_table_page_ = 0; // Alert silence catalog (CAT-024)
        uint32_t network_partition_event_table_page_ = 0; // Network partition event catalog (CAT-024)
        uint32_t network_partition_member_table_page_ = 0; // Network partition member catalog (CAT-024)
        uint32_t healing_policy_table_page_ = 0; // Healing policy catalog (CAT-024)
        uint32_t healing_action_table_page_ = 0; // Healing action catalog (CAT-024)
        uint32_t healing_action_param_table_page_ = 0; // Healing action param catalog (CAT-024)
        uint32_t healing_run_table_page_ = 0; // Healing run catalog (CAT-024)
        uint32_t healing_step_table_page_ = 0; // Healing step catalog (CAT-024)
        uint32_t job_type_table_page_ = 0; // Scheduler job type catalog (CAT-025)
        uint32_t job_type_param_table_page_ = 0; // Scheduler job type param catalog (CAT-025)
        uint32_t job_param_table_page_ = 0; // Scheduler job param catalog (CAT-025)
        uint32_t job_schedule_table_page_ = 0; // Scheduler job schedule catalog (CAT-025)
        uint32_t job_type_policy_table_page_ = 0; // Scheduler job type policy catalog (CAT-025)
        uint32_t remote_connector_table_page_ = 0; // Remote connector catalog (CAT-026)
        uint32_t remote_connector_capability_table_page_ = 0; // Remote capability catalog (CAT-026)
        uint32_t remote_metadata_snapshot_table_page_ = 0; // Remote snapshot catalog (CAT-026)
        uint32_t remote_metadata_object_table_page_ = 0; // Remote metadata object catalog (CAT-026)
        uint32_t remote_metadata_column_table_page_ = 0; // Remote metadata column catalog (CAT-026)
        uint32_t remote_schema_mapping_table_page_ = 0; // Remote schema mapping catalog (CAT-026)
        uint32_t remote_passthrough_policy_table_page_ = 0; // Remote passthrough policy catalog (CAT-026)
        uint32_t remote_prepared_statement_table_page_ = 0; // Remote prepared statement catalog (CAT-026)
        uint32_t remote_txn_binding_table_page_ = 0; // Remote transaction binding catalog (CAT-026)
        uint32_t remote_execution_audit_table_page_ = 0; // Remote execution audit catalog (CAT-026)
        uint32_t remote_error_table_page_ = 0; // Remote error catalog (CAT-026)
        uint32_t publication_table_page_ = 0; // Publication catalog (CAT-028)
        uint32_t publication_table_link_table_page_ = 0; // Publication table mapping catalog (CAT-028)
        uint32_t publication_schema_table_page_ = 0; // Publication schema mapping catalog (CAT-028)
        uint32_t subscription_table_page_ = 0; // Subscription catalog (CAT-028)
        uint32_t subscription_table_link_table_page_ = 0; // Subscription table state catalog (CAT-028)
        uint32_t cluster_fabric_link_table_page_ = 0; // Cluster fabric link catalog (CAT-029)
        uint32_t cluster_fabric_session_table_page_ = 0; // Cluster fabric session catalog (CAT-029)
        uint32_t cluster_fabric_txn_table_page_ = 0; // Cluster fabric txn catalog (CAT-029)
        uint32_t cluster_fabric_task_table_page_ = 0; // Cluster fabric task catalog (CAT-029)
        uint32_t cluster_fabric_task_chunk_table_page_ = 0; // Cluster fabric task chunk catalog (CAT-029)
        uint32_t cluster_fabric_event_table_page_ = 0; // Cluster fabric event catalog (CAT-029)
        uint32_t cluster_fabric_error_table_page_ = 0; // Cluster fabric error catalog (CAT-029)
        uint32_t olap_watermark_table_page_ = 0; // OLAP watermark catalog (CAT-030)
        uint32_t olap_partition_table_page_ = 0; // OLAP partition catalog (CAT-030)
        uint32_t olap_segment_table_page_ = 0; // OLAP segment catalog (CAT-030)
        uint32_t olap_ingest_log_table_page_ = 0; // OLAP ingest log catalog (CAT-030)
        uint32_t cube_table_page_ = 0; // Cube catalog (CAT-030)
        uint32_t cube_dimension_table_page_ = 0; // Cube dimension catalog (CAT-030)
        uint32_t cube_level_table_page_ = 0; // Cube level catalog (CAT-030)
        uint32_t cube_hierarchy_table_page_ = 0; // Cube hierarchy catalog (CAT-030)
        uint32_t cube_hierarchy_level_table_page_ = 0; // Cube hierarchy level catalog (CAT-030)
        uint32_t cube_measure_table_page_ = 0; // Cube measure catalog (CAT-030)
        uint32_t cube_materialization_table_page_ = 0; // Cube materialization catalog (CAT-030)
        uint32_t cube_refresh_policy_table_page_ = 0; // Cube refresh policy catalog (CAT-030)
        uint32_t cube_job_table_page_ = 0; // Cube job catalog (CAT-030)
        uint32_t cube_job_step_table_page_ = 0; // Cube job step catalog (CAT-030)
        uint32_t cube_stats_table_page_ = 0; // Cube stats catalog (CAT-030)
        uint32_t ts_parser_table_page_ = 0; // Text search parser catalog (CAT-031)
        uint32_t ts_template_table_page_ = 0; // Text search template catalog (CAT-031)
        uint32_t ts_dictionary_table_page_ = 0; // Text search dictionary catalog (CAT-031)
        uint32_t ts_config_table_page_ = 0; // Text search config catalog (CAT-031)
        uint32_t ts_config_map_table_page_ = 0; // Text search map catalog (CAT-031)
        uint32_t blob_filter_table_page_ = 0; // Engine-specific blob filter catalog (CAT-032)
        uint32_t trigger_message_table_page_ = 0; // Engine-specific trigger message catalog (CAT-032)
        uint32_t column_drop_history_table_page_ = 0; // Engine-specific column drop history catalog (CAT-032)
        uint32_t sblr_module_table_page_ = 0; // SBLR module catalog (CAT-033)
        uint32_t sblr_plan_table_page_ = 0; // SBLR plan catalog (CAT-033)
        uint32_t sblr_plan_dependency_table_page_ = 0; // SBLR plan dependency catalog (CAT-033)
        uint32_t sblr_statement_norm_table_page_ = 0; // SBLR statement normalization catalog (CAT-033)
        uint32_t sblr_artifact_table_page_ = 0; // SBLR artifact catalog (CAT-033)
        uint32_t sblr_artifact_stats_table_page_ = 0; // SBLR artifact stats catalog (CAT-033)
        uint32_t sblr_compiler_target_table_page_ = 0; // SBLR compiler target catalog (CAT-033)
        uint32_t sblr_compile_queue_table_page_ = 0; // SBLR compile queue catalog (CAT-033)
        uint32_t replication_channel_table_page_ = 0; // Replication channel catalog (CAT-027)
        uint32_t replication_channel_member_table_page_ = 0; // Replication channel member catalog (CAT-027)
        uint32_t replication_origin_table_page_ = 0; // Replication origin catalog (CAT-027)
        uint32_t replication_cursor_table_page_ = 0; // Replication cursor catalog (CAT-027)
        uint32_t replication_origin_progress_table_page_ = 0; // Replication origin progress catalog (CAT-027)
        uint32_t replication_txn_batch_table_page_ = 0; // Replication txn batch catalog (CAT-027)
        uint32_t replication_apply_log_table_page_ = 0; // Replication apply log catalog (CAT-027)
        uint32_t replication_retry_queue_table_page_ = 0; // Replication retry queue catalog (CAT-027)
        uint32_t replication_conflict_table_page_ = 0; // Replication conflict catalog (CAT-027)
        uint32_t replication_split_brain_event_table_page_ = 0; // Replication split brain catalog (CAT-027)
        uint32_t replication_error_table_page_ = 0; // Replication error catalog (CAT-027)
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
        uint32_t prepared_transaction_locks_table_page_ = 0; // Prepared transaction lock snapshots
        uint32_t encryption_keys_table_page_ = 0;   // Encryption keys (Plan 03B)
        uint32_t authkeys_table_page_ = 0;          // AuthKeys (Plan 03)
        uint32_t sessions_table_page_ = 0;          // Sessions (Plan 03)
        uint32_t home_schema_bindings_table_page_ = 0; // Home schema bindings (CAT-009)
        uint32_t search_path_profiles_table_page_ = 0; // Search-path profiles (CAT-009)
        uint32_t search_path_entries_table_page_ = 0;  // Search-path entries (CAT-009)
        uint32_t audit_log_table_page_ = 0;         // Audit log (Plan 03)
        uint32_t security_policy_epoch_table_page_ = 0; // Policy epoch (Plan 03)

        uint64_t security_policy_epoch_ = 0;

        // Internal methods
        auto ensureDatabaseCatalogRecord(const ID& owner_id, ErrorContext* ctx) -> Status;
        auto ensureObjectCatalogRecord(const ID& object_id,
                                       ObjectType object_type,
                                       const ID& schema_id,
                                       const ID& parent_object_id,
                                       const ID& owner_id,
                                       uint64_t created_time,
                                       ErrorContext* ctx) -> Status;
        auto ensureObjectNameCatalogRecord(const ID& object_id,
                                           ObjectType object_type,
                                           const ID& parent_object_id,
                                           const std::string& schema_path,
                                           const std::string& language_code,
                                           const std::string& name_text,
                                           bool name_is_delimited,
                                           uint64_t created_time,
                                           ID* name_id_out,
                                           ErrorContext* ctx) -> Status;
        auto syncObjectCatalogFromCaches(ErrorContext* ctx) -> Status;
        auto resolveTablespaceUuid(uint16_t tablespace_id) const -> ID;
        auto resolveTablespaceId(const ID &tablespace_uuid) const -> uint16_t;
        void resolveTablespaceBindings();
        auto resolveCharsetUuid(uint16_t charset_id) -> ID;
        auto resolveCharsetId(const ID &charset_uuid) -> uint16_t;
        auto resolveTimezoneUuid(uint16_t timezone_id) -> ID;
        auto resolveTimezoneId(const ID &timezone_uuid) -> uint16_t;
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
            uint32_t slot_index; // Physical slot index across the full overflow chain
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
            uint32_t chain_slot_base = 0;
            const uint32_t page_size = db_->page_size();

            if (page_size < sizeof(CatalogHeapPage))
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Catalog heap page size is invalid");
                return {Status::PAGE_CORRUPT, 0, RecordType{}};
            }

            const uint32_t max_records_per_page =
                static_cast<uint32_t>((page_size - sizeof(CatalogHeapPage)) / sizeof(RecordType));

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
                if (heap->free_offset < sizeof(CatalogHeapPage) ||
                    heap->free_offset > page_size ||
                    heap->record_count > max_records_per_page)
                {
                    bp->unpinPage(current_page_id, false, ctx);
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                      "Catalog heap page header is invalid");
                    return {Status::PAGE_CORRUPT, 0, RecordType{}};
                }

                uint32_t offset = sizeof(CatalogHeapPage);
                const uint32_t max_offset = heap->free_offset;

                for (uint32_t i = 0; i < heap->record_count; i++)
                {
                    if (offset + sizeof(RecordType) > max_offset ||
                        offset + sizeof(RecordType) > page_size)
                    {
                        bp->unpinPage(current_page_id, false, ctx);
                        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                          "Catalog heap page record layout is invalid");
                        return {Status::PAGE_CORRUPT, 0, RecordType{}};
                    }

                    auto *record = reinterpret_cast<RecordType *>(
                        reinterpret_cast<uint8_t *>(page_buffer) + offset);

                    if (predicate(*record))
                    {
                        RecordType found = *record;
                        bp->unpinPage(current_page_id, false, ctx);
                        return {Status::OK, chain_slot_base + i, found};
                    }

                    offset += sizeof(RecordType);
                }

                // Move to next page in chain
                uint32_t next_page = heap->next_page;
                chain_slot_base += heap->record_count;
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
            uint32_t current_page_id = page_id;
            uint32_t remaining_slot_index = slot_index;
            const uint32_t page_size = db_->page_size();

            if (page_size < sizeof(CatalogHeapPage))
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Catalog heap page size is invalid");
                return Status::PAGE_CORRUPT;
            }

            const uint32_t max_records_per_page =
                static_cast<uint32_t>((page_size - sizeof(CatalogHeapPage)) / sizeof(RecordType));

            while (current_page_id != 0)
            {
                void *page_buffer = nullptr;
                Status status = bp->pinPage(current_page_id, &page_buffer, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to pin catalog heap page");
                    return status;
                }

                auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
                if (heap->free_offset < sizeof(CatalogHeapPage) ||
                    heap->free_offset > page_size ||
                    heap->record_count > max_records_per_page)
                {
                    bp->unpinPage(current_page_id, false, ctx);
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                      "Catalog heap page header is invalid");
                    return Status::PAGE_CORRUPT;
                }
                if (remaining_slot_index < heap->record_count)
                {
                    uint32_t offset =
                        sizeof(CatalogHeapPage) + (remaining_slot_index * sizeof(RecordType));
                    if (offset + sizeof(RecordType) > heap->free_offset ||
                        offset + sizeof(RecordType) > page_size)
                    {
                        bp->unpinPage(current_page_id, false, ctx);
                        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                          "Catalog heap page record layout is invalid");
                        return Status::PAGE_CORRUPT;
                    }
                    auto *record = reinterpret_cast<RecordType *>(
                        reinterpret_cast<uint8_t *>(page_buffer) + offset);
                    *record = updated_record;
                    return bp->unpinPage(current_page_id, true, ctx); // Mark as dirty
                }

                remaining_slot_index -= heap->record_count;
                uint32_t next_page = heap->next_page;
                bp->unpinPage(current_page_id, false, ctx);
                current_page_id = next_page;
            }

            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid slot index");
            return Status::INVALID_ARGUMENT;
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

        auto schemaCatalogPageForTesting() const -> uint32_t
        {
            return schemas_table_page_;
        }
        auto recoveryRunCatalogPageForTesting() const -> uint32_t
        {
            return recovery_run_table_page_;
        }
        auto transactionLineageCatalogPageForTesting() const -> uint32_t
        {
            return transaction_lineage_event_table_page_;
        }
        auto schemaEpochCatalogPageForTesting() const -> uint32_t
        {
            return schema_epoch_table_page_;
        }
        auto schemaChangePlanCatalogPageForTesting() const -> uint32_t
        {
            return schema_change_plan_table_page_;
        }
        auto schemaChangeEventCatalogPageForTesting() const -> uint32_t
        {
            return schema_change_event_table_page_;
        }
        auto schemaChangeBackfillCatalogPageForTesting() const -> uint32_t
        {
            return schema_change_backfill_progress_table_page_;
        }
        auto schemaChangeCutoverGuardCatalogPageForTesting() const -> uint32_t
        {
            return schema_change_cutover_guard_table_page_;
        }
        auto forensicSnapshotCapsuleCatalogPageForTesting() const -> uint32_t
        {
            return forensic_snapshot_capsule_table_page_;
        }
        auto rawSchemaRecordExistsForTesting(const ID& schema_id, bool& exists,
                                             ErrorContext* ctx) -> Status;
        auto rawSchemaRecordByNameForTesting(const std::string& schema_name,
                                             bool& found,
                                             ID& schema_id_out,
                                             uint32_t& is_valid_out,
                                             uint32_t& page_id_out,
                                             uint32_t& slot_index_out,
                                             ErrorContext* ctx) -> Status;
        auto rawSchemaHeapLayoutForTesting(std::string& summary, ErrorContext* ctx) -> Status;
        auto lastRawSchemaRecordForTesting(ID& schema_id_out,
                                           std::string& schema_name_out,
                                           uint32_t& is_valid_out,
                                           ErrorContext* ctx) -> Status;

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

        auto catalogPageIsOverflowTarget(uint32_t target_page_id,
                                         bool& is_overflow_target,
                                         ErrorContext* ctx) -> Status;
        auto ensureStandaloneCatalogRuntimePage(uint32_t& page_id,
                                                bool& page_changed,
                                                ErrorContext* ctx) -> Status;
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

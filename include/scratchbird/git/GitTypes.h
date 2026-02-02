/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/*
 * ScratchBird Database Engine
 * Git Integration - Type Definitions
 * Copyright (c) 2025 ScratchBird Project
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <chrono>
#include <functional>

namespace scratchbird {
namespace git {

//=============================================================================
// Enumerations
//=============================================================================

/**
 * Git repository state
 */
enum class RepositoryState {
    NOT_INITIALIZED,    // No Git integration configured
    DISCONNECTED,       // Configured but not connected
    CONNECTED,          // Connected to remote
    SYNCING,            // Sync operation in progress
    CONFLICT,           // Conflicts detected
    ERROR               // Error state
};

/**
 * Migration state machine
 */
enum class MigrationState {
    PENDING,            // Not yet applied
    IN_PROGRESS,        // Currently being applied
    APPLIED,            // Successfully applied
    FAILED,             // Application failed
    ROLLED_BACK,        // Was applied but rolled back
    SKIPPED             // Marked as skipped
};

/**
 * Object types for schema export/import
 */
enum class SchemaObjectType {
    TABLE,
    VIEW,
    MATERIALIZED_VIEW,
    INDEX,
    SEQUENCE,
    FUNCTION,
    PROCEDURE,
    TRIGGER,
    DOMAIN,
    TYPE,
    CONSTRAINT,
    GRANT,
    COMMENT,
    SCHEMA
};

/**
 * DDL operation types for change tracking
 */
enum class DDLType {
    CREATE,
    ALTER,
    DROP,
    RENAME,
    TRUNCATE,
    COMMENT,
    GRANT,
    REVOKE
};

/**
 * Conflict resolution strategies
 */
enum class ConflictStrategy {
    GIT_WINS,           // Use Git version
    LOCAL_WINS,         // Keep local version
    MERGE,              // Attempt automatic merge
    PROMPT,             // Interactive resolution
    FAIL                // Abort on conflict
};

/**
 * Diff change types
 */
enum class DiffType {
    ADDED,              // Object exists in target only
    REMOVED,            // Object exists in source only
    MODIFIED,           // Object differs between source and target
    UNCHANGED           // No difference
};

/**
 * Sync direction
 */
enum class SyncDirection {
    EXPORT,             // Database -> Git
    IMPORT,             // Git -> Database
    BIDIRECTIONAL       // Both directions
};

/**
 * Migration naming conventions
 */
enum class MigrationNaming {
    VERSIONED,          // V001__description.sql
    TIMESTAMP,          // 20240301120000__description.sql
    SEQUENTIAL          // 001_description.sql
};

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * Git repository configuration
 */
struct GitConfig {
    std::string url;                // Repository URL
    std::string branch = "main";    // Target branch
    std::string local_path;         // Local clone path

    // Authentication
    std::string ssh_key_path;
    std::string ssh_passphrase;
    std::string username;
    std::string password;
    std::string credential_helper;

    // Behavior
    bool auto_commit = false;
    bool auto_push = false;
    bool auto_pull = true;
    int sync_interval_seconds = 0;  // 0 = manual only

    // Paths
    std::string schema_directory = "schema";
    std::string migrations_directory = "migrations";
    std::string seeds_directory = "seeds";
    std::string config_file = ".scratchbird.yml";
};

/**
 * Schema export/import options
 */
struct SchemaOptions {
    std::vector<std::string> include_schemas;
    std::vector<std::string> exclude_schemas;
    std::vector<std::string> exclude_tables;
    std::vector<std::string> redact_patterns;

    bool include_grants = true;
    bool include_comments = true;
    bool include_defaults = true;
    bool separate_indexes = true;
    bool file_per_object = true;
    bool include_data = false;      // Only for seeds
};

/**
 * Migration configuration
 */
struct MigrationConfig {
    std::string table_name = "SYS$MIGRATIONS";
    MigrationNaming naming = MigrationNaming::VERSIONED;
    bool generate_down = true;
    bool transaction_per_file = true;
    bool checksum_validation = true;
    int statement_timeout_seconds = 300;
};

/**
 * Environment configuration
 */
struct EnvironmentConfig {
    std::string name;
    std::string database;
    std::string host;
    int port = 3092;

    bool approval_required = false;
    bool backup_before_apply = false;
    bool notify_on_apply = false;
    int max_lock_wait_seconds = 60;

    // Restrictions
    bool disallow_drop_table = false;
    bool disallow_drop_column = false;
    bool require_index_for_fk = false;
};

//=============================================================================
// Data Structures
//=============================================================================

/**
 * Represents a schema object for export/import
 */
struct SchemaObject {
    SchemaObjectType type;
    std::string schema_name;
    std::string object_name;
    std::string full_name;          // schema.object
    std::string definition;         // DDL statement
    std::string checksum;           // SHA256 of definition
    std::optional<std::string> comment;
    std::vector<std::string> grants;
    std::vector<std::string> dependencies;

    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point modified_at;
};

/**
 * Represents a migration script
 */
struct Migration {
    std::string version;            // V001, 20240301, etc.
    std::string description;
    std::string filename;
    std::string checksum;           // SHA256 of file content

    std::string up_script;          // @up section
    std::string down_script;        // @down section

    std::vector<std::string> dependencies;
    std::optional<std::string> author;
    std::optional<std::string> date;

    MigrationState state = MigrationState::PENDING;
    std::optional<std::chrono::system_clock::time_point> applied_at;
    std::optional<std::string> applied_by;
    std::optional<int> execution_time_ms;
    std::optional<std::string> error_message;
};

/**
 * DDL change event for tracking
 */
struct DDLEvent {
    std::string event_id;           // UUID
    std::chrono::system_clock::time_point event_time;
    std::string user_name;
    std::optional<std::string> session_id;

    DDLType ddl_type;
    SchemaObjectType object_type;
    std::string schema_name;
    std::string object_name;
    std::string ddl_statement;

    std::optional<std::string> old_definition;
    std::optional<std::string> new_definition;
    std::optional<std::string> git_commit;
    std::optional<std::string> application_name;
    std::optional<std::string> client_ip;
};

/**
 * Schema difference between two states
 */
struct SchemaDiff {
    SchemaObjectType type;
    std::string schema_name;
    std::string object_name;
    DiffType diff_type;

    std::optional<std::string> source_definition;
    std::optional<std::string> target_definition;
    std::string unified_diff;       // Git-style unified diff

    bool has_conflict = false;
    std::string conflict_description;
};

/**
 * Schema conflict for resolution
 */
struct SchemaConflict {
    std::string object_name;
    SchemaObjectType object_type;
    std::string conflict_type;      // COLUMN_MODIFIED, CONSTRAINT_ADDED, etc.

    std::string local_value;
    std::string git_value;

    ConflictStrategy resolution = ConflictStrategy::PROMPT;
    std::optional<std::string> resolved_value;
    bool resolved = false;
};

/**
 * Git commit information
 */
struct GitCommit {
    std::string sha;                // Full SHA
    std::string short_sha;          // Short SHA (7 chars)
    std::string author;
    std::string email;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
    std::vector<std::string> changed_files;
};

/**
 * Git branch information
 */
struct GitBranch {
    std::string name;
    bool is_current = false;
    bool is_remote = false;
    std::optional<std::string> upstream;
    std::optional<GitCommit> head_commit;
    int commits_ahead = 0;
    int commits_behind = 0;
};

/**
 * Git repository status
 */
struct GitStatus {
    RepositoryState state;
    std::string url;
    std::string branch;
    std::string last_commit_sha;
    std::chrono::system_clock::time_point last_sync;

    std::vector<std::string> staged_files;
    std::vector<std::string> modified_files;
    std::vector<std::string> untracked_files;
    std::vector<std::string> conflicted_files;

    int pending_changes = 0;
    bool has_conflicts = false;
    bool needs_push = false;
    bool needs_pull = false;
};

/**
 * Sync operation result
 */
struct SyncResult {
    bool success;
    SyncDirection direction;
    std::chrono::system_clock::time_point timestamp;

    std::string commit_before;
    std::string commit_after;

    int objects_exported = 0;
    int objects_imported = 0;
    int migrations_applied = 0;

    std::vector<std::string> affected_objects;
    std::vector<SchemaConflict> conflicts;
    std::optional<std::string> error_message;
};

/**
 * Migration execution result
 */
struct MigrationResult {
    bool success;
    std::string version;
    std::string description;

    int statements_executed = 0;
    int execution_time_ms = 0;

    std::optional<std::string> error_message;
    std::optional<int> error_line;
    std::optional<std::string> error_statement;
};

//=============================================================================
// Callback Types
//=============================================================================

using ProgressCallback = std::function<void(int current, int total, const std::string& message)>;
using ConflictCallback = std::function<ConflictStrategy(const SchemaConflict& conflict)>;
using ConfirmCallback = std::function<bool(const std::string& message)>;
using LogCallback = std::function<void(const std::string& level, const std::string& message)>;

//=============================================================================
// Error Codes
//=============================================================================

enum class GitErrorCode {
    SUCCESS = 0,

    // Repository errors (GIT0xx)
    GIT001_REPO_NOT_INITIALIZED = 1001,
    GIT002_REMOTE_CONNECTION_FAILED = 1002,
    GIT003_AUTHENTICATION_FAILED = 1003,
    GIT004_BRANCH_NOT_FOUND = 1004,
    GIT005_MERGE_CONFLICT = 1005,
    GIT006_PUSH_REJECTED = 1006,
    GIT007_DIRTY_WORKING_TREE = 1007,
    GIT008_INVALID_URL = 1008,
    GIT009_CLONE_FAILED = 1009,
    GIT010_CHECKOUT_FAILED = 1010,

    // Migration errors (MIG0xx)
    MIG001_FILE_NOT_FOUND = 2001,
    MIG002_ALREADY_APPLIED = 2002,
    MIG003_CHECKSUM_MISMATCH = 2003,
    MIG004_DEPENDENCY_NOT_MET = 2004,
    MIG005_EXECUTION_FAILED = 2005,
    MIG006_ROLLBACK_NOT_POSSIBLE = 2006,
    MIG007_LOCK_HELD = 2007,
    MIG008_INVALID_FORMAT = 2008,
    MIG009_VERSION_GAP = 2009,
    MIG010_OUT_OF_ORDER = 2010,

    // Schema errors (SCH0xx)
    SCH001_EXPORT_FAILED = 3001,
    SCH002_IMPORT_FAILED = 3002,
    SCH003_OBJECT_NOT_FOUND = 3003,
    SCH004_INVALID_DDL = 3004,
    SCH005_CONFLICT_UNRESOLVED = 3005,

    // General errors
    INTERNAL_ERROR = 9999
};

/**
 * Git integration exception
 */
class GitException : public std::exception {
public:
    GitException(GitErrorCode code, const std::string& message)
        : code_(code), message_(message) {}

    const char* what() const noexcept override { return message_.c_str(); }
    GitErrorCode code() const { return code_; }

private:
    GitErrorCode code_;
    std::string message_;
};

//=============================================================================
// Utility Functions
//=============================================================================

inline const char* toString(SchemaObjectType type) {
    switch (type) {
        case SchemaObjectType::TABLE: return "TABLE";
        case SchemaObjectType::VIEW: return "VIEW";
        case SchemaObjectType::MATERIALIZED_VIEW: return "MATERIALIZED_VIEW";
        case SchemaObjectType::INDEX: return "INDEX";
        case SchemaObjectType::SEQUENCE: return "SEQUENCE";
        case SchemaObjectType::FUNCTION: return "FUNCTION";
        case SchemaObjectType::PROCEDURE: return "PROCEDURE";
        case SchemaObjectType::TRIGGER: return "TRIGGER";
        case SchemaObjectType::DOMAIN: return "DOMAIN";
        case SchemaObjectType::TYPE: return "TYPE";
        case SchemaObjectType::CONSTRAINT: return "CONSTRAINT";
        case SchemaObjectType::GRANT: return "GRANT";
        case SchemaObjectType::COMMENT: return "COMMENT";
        case SchemaObjectType::SCHEMA: return "SCHEMA";
        default: return "UNKNOWN";
    }
}

inline const char* toString(DDLType type) {
    switch (type) {
        case DDLType::CREATE: return "CREATE";
        case DDLType::ALTER: return "ALTER";
        case DDLType::DROP: return "DROP";
        case DDLType::RENAME: return "RENAME";
        case DDLType::TRUNCATE: return "TRUNCATE";
        case DDLType::COMMENT: return "COMMENT";
        case DDLType::GRANT: return "GRANT";
        case DDLType::REVOKE: return "REVOKE";
        default: return "UNKNOWN";
    }
}

inline const char* toString(MigrationState state) {
    switch (state) {
        case MigrationState::PENDING: return "PENDING";
        case MigrationState::IN_PROGRESS: return "IN_PROGRESS";
        case MigrationState::APPLIED: return "APPLIED";
        case MigrationState::FAILED: return "FAILED";
        case MigrationState::ROLLED_BACK: return "ROLLED_BACK";
        case MigrationState::SKIPPED: return "SKIPPED";
        default: return "UNKNOWN";
    }
}

inline const char* toString(DiffType type) {
    switch (type) {
        case DiffType::ADDED: return "ADDED";
        case DiffType::REMOVED: return "REMOVED";
        case DiffType::MODIFIED: return "MODIFIED";
        case DiffType::UNCHANGED: return "UNCHANGED";
        default: return "UNKNOWN";
    }
}

inline const char* toString(ConflictStrategy strategy) {
    switch (strategy) {
        case ConflictStrategy::GIT_WINS: return "GIT_WINS";
        case ConflictStrategy::LOCAL_WINS: return "LOCAL_WINS";
        case ConflictStrategy::MERGE: return "MERGE";
        case ConflictStrategy::PROMPT: return "PROMPT";
        case ConflictStrategy::FAIL: return "FAIL";
        default: return "UNKNOWN";
    }
}

inline const char* getDirectoryForType(SchemaObjectType type) {
    switch (type) {
        case SchemaObjectType::TABLE: return "tables";
        case SchemaObjectType::VIEW: return "views";
        case SchemaObjectType::MATERIALIZED_VIEW: return "views";
        case SchemaObjectType::INDEX: return "indexes";
        case SchemaObjectType::SEQUENCE: return "sequences";
        case SchemaObjectType::FUNCTION: return "functions";
        case SchemaObjectType::PROCEDURE: return "procedures";
        case SchemaObjectType::TRIGGER: return "triggers";
        case SchemaObjectType::DOMAIN: return "domains";
        case SchemaObjectType::TYPE: return "types";
        default: return "other";
    }
}

} // namespace git
} // namespace scratchbird

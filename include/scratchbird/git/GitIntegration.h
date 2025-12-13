/*
 * ScratchBird Database Engine
 * Git Integration - Main Entry Point
 * Copyright (c) 2025 ScratchBird Project
 */
#pragma once

#include "GitTypes.h"
#include "GitRepository.h"
#include "SchemaExporter.h"
#include "SchemaImporter.h"
#include "MigrationManager.h"
#include "DDLTracker.h"
#include "GitConfigParser.h"
#include <memory>

namespace scratchbird {

// Forward declarations
namespace core {
class Catalog;
class Database;
class Session;
}

namespace git {

/**
 * GitIntegration is the main entry point for Git-based schema versioning.
 *
 * Coordinates:
 * - GitRepository for Git operations
 * - SchemaExporter for exporting DDL
 * - SchemaImporter for importing DDL
 * - MigrationManager for migration lifecycle
 * - DDLTracker for change capture
 *
 * Provides unified interface for:
 * - INIT GIT REPOSITORY
 * - EXPORT SCHEMA TO GIT
 * - IMPORT SCHEMA FROM GIT
 * - GENERATE/APPLY/ROLLBACK MIGRATION
 * - SHOW GIT STATUS/DIFF
 */
class GitIntegration {
public:
    /**
     * Constructor
     * @param catalog Database catalog
     */
    explicit GitIntegration(core::Catalog& catalog);
    ~GitIntegration();

    // Non-copyable
    GitIntegration(const GitIntegration&) = delete;
    GitIntegration& operator=(const GitIntegration&) = delete;

    //=========================================================================
    // Initialization
    //=========================================================================

    /**
     * Initialize Git integration with URL
     * @param url Repository URL
     * @param branch Target branch
     * @param options Additional options
     * @return true on success
     */
    bool initialize(const std::string& url,
                    const std::string& branch = "main",
                    const std::map<std::string, std::string>& options = {});

    /**
     * Initialize from configuration file
     * @param config_path Path to .scratchbird.yml
     * @return true on success
     */
    bool initializeFromConfig(const std::string& config_path);

    /**
     * Check if Git integration is initialized
     */
    bool isInitialized() const;

    /**
     * Close Git integration
     */
    void close();

    //=========================================================================
    // Status and Info
    //=========================================================================

    /**
     * Get Git status
     * @return Current status
     */
    GitStatus getStatus() const;

    /**
     * Show status (formatted for display)
     * @return Formatted status string
     */
    std::string showStatus() const;

    /**
     * Get current branch
     */
    std::string getCurrentBranch() const;

    /**
     * Get repository URL
     */
    std::string getRepositoryUrl() const;

    //=========================================================================
    // Remote Operations
    //=========================================================================

    /**
     * Pull changes from remote
     * @param progress Progress callback
     * @return true on success
     */
    bool pull(ProgressCallback progress = nullptr);

    /**
     * Push changes to remote
     * @param progress Progress callback
     * @return true on success
     */
    bool push(ProgressCallback progress = nullptr);

    /**
     * Fetch from remote without merging
     * @param progress Progress callback
     * @return true on success
     */
    bool fetch(ProgressCallback progress = nullptr);

    //=========================================================================
    // Branch Operations
    //=========================================================================

    /**
     * Checkout a branch
     * @param branch Branch name
     * @return true on success
     */
    bool checkout(const std::string& branch);

    /**
     * Create a new branch
     * @param branch Branch name
     * @param checkout Switch to new branch
     * @return true on success
     */
    bool createBranch(const std::string& branch, bool checkout = true);

    /**
     * List branches
     * @param include_remote Include remote branches
     * @return List of branches
     */
    std::vector<GitBranch> listBranches(bool include_remote = false) const;

    //=========================================================================
    // Schema Export
    //=========================================================================

    /**
     * Export schema to Git
     * @param options Export options
     * @param commit_message Optional commit message
     * @param progress Progress callback
     * @return Sync result
     */
    SyncResult exportSchema(const SchemaOptions& options = {},
                            const std::string& commit_message = "",
                            ProgressCallback progress = nullptr);

    /**
     * Export specific schema
     * @param schema_name Schema to export
     * @param options Export options
     * @param commit_message Optional commit message
     * @return Sync result
     */
    SyncResult exportSchemaName(const std::string& schema_name,
                                const SchemaOptions& options = {},
                                const std::string& commit_message = "");

    /**
     * Export specific objects
     * @param objects List of schema.object names
     * @param options Export options
     * @param commit_message Optional commit message
     * @return Sync result
     */
    SyncResult exportObjects(const std::vector<std::string>& objects,
                             const SchemaOptions& options = {},
                             const std::string& commit_message = "");

    /**
     * Dry run export (show what would be exported)
     * @param options Export options
     * @return List of objects
     */
    std::vector<SchemaObject> exportDryRun(const SchemaOptions& options = {});

    //=========================================================================
    // Schema Import
    //=========================================================================

    /**
     * Import schema from Git
     * @param options Import options
     * @param conflict_handler Conflict resolution callback
     * @param progress Progress callback
     * @return Sync result
     */
    SyncResult importSchema(const SchemaOptions& options = {},
                            ConflictCallback conflict_handler = nullptr,
                            ProgressCallback progress = nullptr);

    /**
     * Import from specific branch
     * @param branch Branch name
     * @param options Import options
     * @param conflict_handler Conflict resolution callback
     * @return Sync result
     */
    SyncResult importFromBranch(const std::string& branch,
                                const SchemaOptions& options = {},
                                ConflictCallback conflict_handler = nullptr);

    /**
     * Dry run import (show what would be imported)
     * @param options Import options
     * @return List of changes
     */
    std::vector<SchemaDiff> importDryRun(const SchemaOptions& options = {});

    //=========================================================================
    // Schema Diff
    //=========================================================================

    /**
     * Get diff between database and Git
     * @return List of differences
     */
    std::vector<SchemaDiff> getDiff();

    /**
     * Get diff for specific object
     * @param schema_name Schema name
     * @param object_name Object name
     * @param type Object type
     * @return Diff or nullopt
     */
    std::optional<SchemaDiff> getObjectDiff(const std::string& schema_name,
                                             const std::string& object_name,
                                             SchemaObjectType type);

    /**
     * Show diff (formatted for display)
     * @return Formatted diff string
     */
    std::string showDiff();

    /**
     * Show diff for specific object (formatted)
     * @param schema_name Schema name
     * @param object_name Object name
     * @return Formatted diff string
     */
    std::string showObjectDiff(const std::string& schema_name,
                                const std::string& object_name);

    /**
     * Get diff between branches
     * @param branch1 First branch
     * @param branch2 Second branch
     * @return List of differences
     */
    std::vector<SchemaDiff> getBranchDiff(const std::string& branch1,
                                           const std::string& branch2);

    //=========================================================================
    // Migration Management
    //=========================================================================

    /**
     * Generate a new migration
     * @param description Migration description
     * @return Generated migration
     */
    Migration generateMigration(const std::string& description);

    /**
     * Generate migration from current diff
     * @param description Migration description
     * @return Generated migration
     */
    Migration generateMigrationFromDiff(const std::string& description);

    /**
     * Get pending migrations
     * @return List of pending migrations
     */
    std::vector<Migration> getPendingMigrations();

    /**
     * Get migration history
     * @return List of all migrations
     */
    std::vector<Migration> getMigrationHistory();

    /**
     * Show pending migrations (formatted)
     * @return Formatted string
     */
    std::string showPendingMigrations();

    /**
     * Show migration history (formatted)
     * @return Formatted string
     */
    std::string showMigrationHistory();

    /**
     * Apply all pending migrations
     * @param progress Progress callback
     * @return Results for each migration
     */
    std::vector<MigrationResult> applyMigrations(
        ProgressCallback progress = nullptr);

    /**
     * Apply specific migration
     * @param version Migration version
     * @return Execution result
     */
    MigrationResult applyMigration(const std::string& version);

    /**
     * Apply migrations up to version
     * @param target_version Target version
     * @param progress Progress callback
     * @return Results
     */
    std::vector<MigrationResult> applyMigrationsTo(
        const std::string& target_version,
        ProgressCallback progress = nullptr);

    /**
     * Rollback last migration
     * @return Execution result
     */
    MigrationResult rollbackMigration();

    /**
     * Rollback to specific version
     * @param target_version Target version
     * @param progress Progress callback
     * @return Results
     */
    std::vector<MigrationResult> rollbackMigrationsTo(
        const std::string& target_version,
        ProgressCallback progress = nullptr);

    /**
     * Validate all migrations
     * @return List of validation errors
     */
    std::vector<std::string> validateMigrations();

    //=========================================================================
    // Commit Operations
    //=========================================================================

    /**
     * Commit current changes
     * @param message Commit message
     * @return Commit SHA on success
     */
    std::string commit(const std::string& message);

    /**
     * Get uncommitted changes
     * @return List of uncommitted DDL events
     */
    std::vector<DDLEvent> getUncommittedChanges();

    /**
     * Show uncommitted changes (formatted)
     * @return Formatted string
     */
    std::string showUncommittedChanges();

    /**
     * Discard uncommitted changes
     * @return Number of changes discarded
     */
    int discardChanges();

    /**
     * Discard specific change
     * @param event_id Event ID
     * @return true on success
     */
    bool discardChange(const std::string& event_id);

    //=========================================================================
    // Conflict Resolution
    //=========================================================================

    /**
     * Get current conflicts
     * @return List of conflicts
     */
    std::vector<SchemaConflict> getConflicts();

    /**
     * Show conflicts (formatted)
     * @return Formatted string
     */
    std::string showConflicts();

    /**
     * Resolve conflict
     * @param object_name Object with conflict
     * @param strategy Resolution strategy
     * @param custom_resolution Custom DDL (for manual resolution)
     * @return true on success
     */
    bool resolveConflict(const std::string& object_name,
                         ConflictStrategy strategy,
                         const std::string& custom_resolution = "");

    //=========================================================================
    // DDL Tracking
    //=========================================================================

    /**
     * Enable DDL tracking
     */
    void enableTracking();

    /**
     * Disable DDL tracking
     */
    void disableTracking();

    /**
     * Check if DDL tracking is enabled
     */
    bool isTrackingEnabled() const;

    /**
     * Record a DDL change
     * @param session Session executing DDL
     * @param ddl_type DDL type
     * @param object_type Object type
     * @param schema_name Schema name
     * @param object_name Object name
     * @param ddl_statement DDL statement
     * @return Event ID
     */
    std::string trackDDL(core::Session& session,
                         DDLType ddl_type,
                         SchemaObjectType object_type,
                         const std::string& schema_name,
                         const std::string& object_name,
                         const std::string& ddl_statement);

    //=========================================================================
    // Environment Support
    //=========================================================================

    /**
     * Get current environment
     */
    std::string getCurrentEnvironment() const;

    /**
     * Set current environment
     * @param env_name Environment name
     * @return true on success
     */
    bool setEnvironment(const std::string& env_name);

    /**
     * List available environments
     */
    std::vector<std::string> listEnvironments() const;

    /**
     * Apply migrations to specific environment
     * @param env_name Environment name
     * @param progress Progress callback
     * @return Results
     */
    std::vector<MigrationResult> applyToEnvironment(
        const std::string& env_name,
        ProgressCallback progress = nullptr);

    //=========================================================================
    // Configuration
    //=========================================================================

    /**
     * Get configuration
     */
    const GitConfig& getConfig() const;

    /**
     * Update configuration
     */
    void setConfig(const GitConfig& config);

    /**
     * Get configuration parser
     */
    GitConfigParser& getConfigParser();

    /**
     * Reload configuration from file
     * @return true on success
     */
    bool reloadConfig();

    //=========================================================================
    // Component Access
    //=========================================================================

    /**
     * Get repository component
     */
    GitRepository& getRepository();

    /**
     * Get schema exporter
     */
    SchemaExporter& getExporter();

    /**
     * Get schema importer
     */
    SchemaImporter& getImporter();

    /**
     * Get migration manager
     */
    MigrationManager& getMigrationManager();

    /**
     * Get DDL tracker
     */
    DDLTracker& getDDLTracker();

    //=========================================================================
    // Error Handling
    //=========================================================================

    /**
     * Set log callback
     */
    void setLogCallback(LogCallback callback);

    /**
     * Get last error message
     */
    std::string getLastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    core::Catalog& catalog_;
    std::unique_ptr<GitConfigParser> config_parser_;
    std::unique_ptr<GitRepository> repository_;
    std::unique_ptr<SchemaExporter> exporter_;
    std::unique_ptr<SchemaImporter> importer_;
    std::unique_ptr<MigrationManager> migration_manager_;
    std::unique_ptr<DDLTracker> ddl_tracker_;

    GitConfig config_;
    std::string current_environment_;
    bool initialized_ = false;
    LogCallback log_callback_;
    std::string last_error_;

    void setError(const std::string& error);
    void log(const std::string& level, const std::string& message);
    void initializeComponents();
};

} // namespace git
} // namespace scratchbird

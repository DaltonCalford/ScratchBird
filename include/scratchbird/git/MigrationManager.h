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
 * Migration Manager - Track and Execute Database Migrations
 * Copyright (c) 2025 ScratchBird Project
 */
#pragma once

#include "GitTypes.h"
#include "GitRepository.h"
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
 * MigrationManager handles database migration lifecycle.
 *
 * Features:
 * - Parse and validate migration files
 * - Track applied migrations in SYS$MIGRATIONS table
 * - Execute migrations in correct order
 * - Support rollback operations
 * - Generate new migrations from schema diff
 * - Handle migration locks to prevent concurrent execution
 */
class MigrationManager {
public:
    /**
     * Constructor
     * @param catalog Database catalog
     * @param repository Git repository containing migrations
     * @param config Migration configuration
     */
    MigrationManager(core::Catalog& catalog,
                     GitRepository& repository,
                     const MigrationConfig& config = {});
    ~MigrationManager();

    // Non-copyable
    MigrationManager(const MigrationManager&) = delete;
    MigrationManager& operator=(const MigrationManager&) = delete;

    //=========================================================================
    // Initialization
    //=========================================================================

    /**
     * Initialize migration system (create tracking tables)
     * @return true on success
     */
    bool initialize();

    /**
     * Check if migration system is initialized
     */
    bool isInitialized() const;

    /**
     * Get current schema version
     * @return Latest applied migration version or empty
     */
    std::string getCurrentVersion() const;

    //=========================================================================
    // Migration Discovery
    //=========================================================================

    /**
     * Scan for migration files in repository
     * @return List of all migrations
     */
    std::vector<Migration> scanMigrations();

    /**
     * Get pending migrations (not yet applied)
     * @return List of pending migrations
     */
    std::vector<Migration> getPendingMigrations();

    /**
     * Get applied migrations
     * @return List of applied migrations
     */
    std::vector<Migration> getAppliedMigrations();

    /**
     * Get migration by version
     * @param version Migration version (V001, etc.)
     * @return Migration or nullopt
     */
    std::optional<Migration> getMigration(const std::string& version);

    /**
     * Get migration history (applied + pending in order)
     * @return Full migration history
     */
    std::vector<Migration> getHistory();

    //=========================================================================
    // Migration Execution
    //=========================================================================

    /**
     * Apply all pending migrations
     * @param progress Progress callback
     * @return Results for each migration
     */
    std::vector<MigrationResult> applyAll(ProgressCallback progress = nullptr);

    /**
     * Apply migrations up to a specific version
     * @param target_version Target version (inclusive)
     * @param progress Progress callback
     * @return Results for each migration
     */
    std::vector<MigrationResult> applyTo(const std::string& target_version,
                                          ProgressCallback progress = nullptr);

    /**
     * Apply a single migration
     * @param version Migration version to apply
     * @return Execution result
     */
    MigrationResult apply(const std::string& version);

    /**
     * Apply migration with session
     * @param migration Migration to apply
     * @param session Database session for execution
     * @return Execution result
     */
    MigrationResult applyMigration(const Migration& migration,
                                    core::Session& session);

    /**
     * Skip a migration (mark as applied without executing)
     * @param version Migration version to skip
     * @param reason Reason for skipping
     * @return true on success
     */
    bool skip(const std::string& version, const std::string& reason = "");

    //=========================================================================
    // Rollback Operations
    //=========================================================================

    /**
     * Rollback the last applied migration
     * @return Execution result
     */
    MigrationResult rollbackLast();

    /**
     * Rollback multiple migrations
     * @param count Number of migrations to rollback
     * @param progress Progress callback
     * @return Results for each rollback
     */
    std::vector<MigrationResult> rollback(int count,
                                           ProgressCallback progress = nullptr);

    /**
     * Rollback to a specific version (exclusive)
     * @param target_version Target version to rollback to
     * @param progress Progress callback
     * @return Results for each rollback
     */
    std::vector<MigrationResult> rollbackTo(const std::string& target_version,
                                             ProgressCallback progress = nullptr);

    /**
     * Check if a migration can be rolled back
     * @param version Migration version
     * @return true if rollback is possible
     */
    bool canRollback(const std::string& version) const;

    //=========================================================================
    // Migration Generation
    //=========================================================================

    /**
     * Generate a new empty migration
     * @param description Migration description
     * @return Generated migration
     */
    Migration generate(const std::string& description);

    /**
     * Generate migration from schema diff
     * @param description Migration description
     * @param diffs Schema differences to include
     * @return Generated migration
     */
    Migration generateFromDiff(const std::string& description,
                               const std::vector<SchemaDiff>& diffs);

    /**
     * Generate migration from template
     * @param description Migration description
     * @param template_name Template name
     * @param parameters Template parameters
     * @return Generated migration
     */
    Migration generateFromTemplate(const std::string& description,
                                    const std::string& template_name,
                                    const std::map<std::string, std::string>& parameters = {});

    /**
     * Save migration to file
     * @param migration Migration to save
     * @return File path
     */
    std::string saveMigration(const Migration& migration);

    //=========================================================================
    // Validation
    //=========================================================================

    /**
     * Validate all migrations
     * @return List of validation errors
     */
    std::vector<std::string> validate();

    /**
     * Validate a specific migration
     * @param version Migration version
     * @return Validation error or empty string
     */
    std::string validateMigration(const std::string& version);

    /**
     * Validate checksums of applied migrations
     * @return List of migrations with checksum mismatches
     */
    std::vector<Migration> validateChecksums();

    /**
     * Check for version gaps in applied migrations
     * @return List of missing versions
     */
    std::vector<std::string> checkVersionGaps();

    /**
     * Check for out-of-order migrations
     * @return List of out-of-order versions
     */
    std::vector<std::string> checkOutOfOrder();

    //=========================================================================
    // Locking
    //=========================================================================

    /**
     * Acquire migration lock
     * @param reason Lock reason
     * @param timeout_seconds Lock timeout
     * @return true if lock acquired
     */
    bool acquireLock(const std::string& reason = "",
                     int timeout_seconds = 60);

    /**
     * Release migration lock
     * @return true if lock was held and released
     */
    bool releaseLock();

    /**
     * Force release migration lock (admin only)
     * @return true if lock was released
     */
    bool forceReleaseLock();

    /**
     * Check if migration lock is held
     * @return Lock holder info or nullopt if not locked
     */
    std::optional<std::pair<std::string, std::chrono::system_clock::time_point>>
        getLockInfo() const;

    //=========================================================================
    // Repair Operations
    //=========================================================================

    /**
     * Re-baseline migrations at a specific version
     * @param version Version to baseline at
     * @return true on success
     */
    bool baseline(const std::string& version);

    /**
     * Repair migration checksum
     * @param version Migration version
     * @param checksum Correct checksum
     * @return true on success
     */
    bool repairChecksum(const std::string& version,
                        const std::string& checksum);

    /**
     * Remove migration from history
     * @param version Migration version
     * @return true on success
     */
    bool removeFromHistory(const std::string& version);

    //=========================================================================
    // Dependency Management
    //=========================================================================

    /**
     * Get migration dependency graph
     * @return Map of version -> dependencies
     */
    std::map<std::string, std::vector<std::string>> getDependencyGraph();

    /**
     * Check if dependencies are met for a migration
     * @param version Migration version
     * @return true if all dependencies are applied
     */
    bool dependenciesMet(const std::string& version);

    /**
     * Get migrations blocking a specific version
     * @param version Target version
     * @return List of blocking migration versions
     */
    std::vector<std::string> getBlockingMigrations(const std::string& version);

    //=========================================================================
    // Configuration
    //=========================================================================

    /**
     * Set migration configuration
     */
    void setConfig(const MigrationConfig& config);

    /**
     * Get migration configuration
     */
    const MigrationConfig& getConfig() const;

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
    GitRepository& repository_;
    MigrationConfig config_;
    LogCallback log_callback_;
    std::string last_error_;
    bool lock_held_ = false;

    void setError(const std::string& error);
    void log(const std::string& level, const std::string& message);

    Migration parseMigrationFile(const std::string& path);
    std::string generateVersion();
    std::string generateFileName(const std::string& version,
                                  const std::string& description);
    std::string calculateChecksum(const std::string& content);
    bool executeScript(const std::string& script, core::Session& session);
    bool recordMigration(const Migration& migration, bool success,
                         const std::string& error = "");
    bool recordRollback(const std::string& version);
    std::vector<Migration> sortByDependencies(
        const std::vector<Migration>& migrations);
};

} // namespace git
} // namespace scratchbird

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
 * Schema Importer - Apply DDL from Git Repository
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
}

namespace git {

/**
 * SchemaImporter applies schema definitions from Git repository files to the database.
 *
 * Features:
 * - Import tables, views, indexes, functions, etc. from SQL files
 * - Validate DDL before applying
 * - Detect and resolve conflicts
 * - Support dry-run mode
 * - Handle dependencies and ordering
 */
class SchemaImporter {
public:
    /**
     * Constructor
     * @param catalog Database catalog to import into
     * @param repository Git repository to import from
     */
    SchemaImporter(core::Catalog& catalog, GitRepository& repository);
    ~SchemaImporter();

    // Non-copyable
    SchemaImporter(const SchemaImporter&) = delete;
    SchemaImporter& operator=(const SchemaImporter&) = delete;

    //=========================================================================
    // Import Operations
    //=========================================================================

    /**
     * Import entire schema from Git
     * @param options Import options
     * @param conflict_handler Conflict resolution callback
     * @param progress Progress callback
     * @return Sync result
     */
    SyncResult importAll(const SchemaOptions& options = {},
                         ConflictCallback conflict_handler = nullptr,
                         ProgressCallback progress = nullptr);

    /**
     * Import specific schema (namespace)
     * @param schema_name Schema name to import
     * @param options Import options
     * @param conflict_handler Conflict resolution callback
     * @param progress Progress callback
     * @return Sync result
     */
    SyncResult importSchema(const std::string& schema_name,
                            const SchemaOptions& options = {},
                            ConflictCallback conflict_handler = nullptr,
                            ProgressCallback progress = nullptr);

    /**
     * Import specific objects
     * @param objects List of schema.object names
     * @param options Import options
     * @param conflict_handler Conflict resolution callback
     * @param progress Progress callback
     * @return Sync result
     */
    SyncResult importObjects(const std::vector<std::string>& objects,
                             const SchemaOptions& options = {},
                             ConflictCallback conflict_handler = nullptr,
                             ProgressCallback progress = nullptr);

    /**
     * Import from a specific branch
     * @param branch_name Branch name
     * @param options Import options
     * @param conflict_handler Conflict resolution callback
     * @param progress Progress callback
     * @return Sync result
     */
    SyncResult importFromBranch(const std::string& branch_name,
                                const SchemaOptions& options = {},
                                ConflictCallback conflict_handler = nullptr,
                                ProgressCallback progress = nullptr);

    /**
     * Dry run - show what would be imported without applying
     * @param options Import options
     * @return List of changes that would be applied
     */
    std::vector<SchemaDiff> dryRun(const SchemaOptions& options = {});

    //=========================================================================
    // Validation
    //=========================================================================

    /**
     * Validate DDL files without importing
     * @param options Import options
     * @return List of validation errors (empty if valid)
     */
    std::vector<std::string> validate(const SchemaOptions& options = {});

    /**
     * Check for syntax errors in a DDL file
     * @param file_path Path to DDL file
     * @return Error message or empty string if valid
     */
    std::string validateFile(const std::string& file_path);

    /**
     * Check dependency order
     * @return List of objects with unmet dependencies
     */
    std::vector<std::string> checkDependencies();

    //=========================================================================
    // Conflict Detection and Resolution
    //=========================================================================

    /**
     * Detect conflicts between Git and database
     * @param options Import options
     * @return List of conflicts
     */
    std::vector<SchemaConflict> detectConflicts(const SchemaOptions& options = {});

    /**
     * Set conflict resolution strategy per object type
     * @param type Object type
     * @param strategy Resolution strategy
     */
    void setConflictStrategy(SchemaObjectType type, ConflictStrategy strategy);

    /**
     * Set default conflict resolution strategy
     * @param strategy Resolution strategy
     */
    void setDefaultConflictStrategy(ConflictStrategy strategy);

    /**
     * Resolve a specific conflict
     * @param conflict Conflict to resolve
     * @param strategy Resolution strategy
     * @param custom_resolution Custom DDL (for CUSTOM strategy)
     * @return true on success
     */
    bool resolveConflict(SchemaConflict& conflict,
                         ConflictStrategy strategy,
                         const std::string& custom_resolution = "");

    /**
     * Get unresolved conflicts
     * @return List of unresolved conflicts
     */
    std::vector<SchemaConflict> getUnresolvedConflicts() const;

    //=========================================================================
    // Diff Operations
    //=========================================================================

    /**
     * Get diff between Git and database
     * @param options Import options
     * @return List of differences
     */
    std::vector<SchemaDiff> getDiff(const SchemaOptions& options = {});

    /**
     * Get diff for a specific object
     * @param schema_name Schema name
     * @param object_name Object name
     * @param type Object type
     * @return Diff info or nullopt if identical
     */
    std::optional<SchemaDiff> getObjectDiff(const std::string& schema_name,
                                             const std::string& object_name,
                                             SchemaObjectType type);

    /**
     * Get diff between two branches
     * @param branch1 First branch
     * @param branch2 Second branch
     * @return List of differences
     */
    std::vector<SchemaDiff> getBranchDiff(const std::string& branch1,
                                           const std::string& branch2);

    //=========================================================================
    // Object Parsing
    //=========================================================================

    /**
     * Parse schema objects from Git repository
     * @param options Import options
     * @return List of parsed objects
     */
    std::vector<SchemaObject> parseObjects(const SchemaOptions& options = {});

    /**
     * Parse a single DDL file
     * @param file_path Path to DDL file
     * @return Parsed object or nullopt on error
     */
    std::optional<SchemaObject> parseFile(const std::string& file_path);

    /**
     * Parse DDL string
     * @param ddl DDL statement
     * @return Parsed object or nullopt on error
     */
    std::optional<SchemaObject> parseDDL(const std::string& ddl);

    //=========================================================================
    // Dependency Management
    //=========================================================================

    /**
     * Get objects sorted by dependencies
     * @param objects Objects to sort
     * @return Topologically sorted objects
     */
    std::vector<SchemaObject> sortByDependencies(
        const std::vector<SchemaObject>& objects);

    /**
     * Get dependencies for an object
     * @param object Object to analyze
     * @return List of dependency names
     */
    std::vector<std::string> getDependencies(const SchemaObject& object);

    /**
     * Check if an object can be applied (dependencies met)
     * @param object Object to check
     * @return true if all dependencies exist
     */
    bool canApply(const SchemaObject& object);

    //=========================================================================
    // Apply Operations
    //=========================================================================

    /**
     * Apply a single object
     * @param object Object to apply
     * @return true on success
     */
    bool applyObject(const SchemaObject& object);

    /**
     * Apply DDL statement
     * @param ddl DDL to execute
     * @return true on success
     */
    bool applyDDL(const std::string& ddl);

    /**
     * Rollback the last import
     * @return true on success
     */
    bool rollback();

    //=========================================================================
    // Configuration
    //=========================================================================

    /**
     * Set default import options
     */
    void setDefaultOptions(const SchemaOptions& options);

    /**
     * Get default import options
     */
    const SchemaOptions& getDefaultOptions() const;

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
    SchemaOptions default_options_;
    std::map<SchemaObjectType, ConflictStrategy> conflict_strategies_;
    ConflictStrategy default_conflict_strategy_ = ConflictStrategy::PROMPT;
    std::vector<SchemaConflict> pending_conflicts_;
    LogCallback log_callback_;
    std::string last_error_;

    void setError(const std::string& error);
    void log(const std::string& level, const std::string& message);

    bool parseFileHeader(const std::string& content, SchemaObject& object);
    std::string generateUnifiedDiff(const std::string& source,
                                     const std::string& target);
    ConflictStrategy getStrategyForType(SchemaObjectType type);
    std::vector<SchemaObject> listGitObjects(const SchemaOptions& options);
    std::vector<SchemaObject> listDatabaseObjects(const SchemaOptions& options);
};

} // namespace git
} // namespace scratchbird

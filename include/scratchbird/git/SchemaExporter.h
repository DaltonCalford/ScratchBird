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
 * Schema Exporter - Export DDL to Git Repository
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
 * SchemaExporter exports database schema objects to files in a Git repository.
 *
 * Features:
 * - Export tables, views, indexes, functions, etc. to individual SQL files
 * - Include grants, comments, and default values
 * - Generate checksums for change detection
 * - Support incremental export (only changed objects)
 * - Follow standard directory layout (schema/tables/, schema/views/, etc.)
 */
class SchemaExporter {
public:
    /**
     * Constructor
     * @param catalog Database catalog to export from
     * @param repository Git repository to export to
     */
    SchemaExporter(core::Catalog& catalog, GitRepository& repository);
    ~SchemaExporter();

    // Non-copyable
    SchemaExporter(const SchemaExporter&) = delete;
    SchemaExporter& operator=(const SchemaExporter&) = delete;

    //=========================================================================
    // Export Operations
    //=========================================================================

    /**
     * Export entire database schema
     * @param options Export options
     * @param progress Progress callback
     * @return Sync result
     */
    SyncResult exportAll(const SchemaOptions& options = {},
                         ProgressCallback progress = nullptr);

    /**
     * Export specific schema (namespace)
     * @param schema_name Schema name to export
     * @param options Export options
     * @param progress Progress callback
     * @return Sync result
     */
    SyncResult exportSchema(const std::string& schema_name,
                            const SchemaOptions& options = {},
                            ProgressCallback progress = nullptr);

    /**
     * Export specific objects
     * @param objects List of schema.object names
     * @param options Export options
     * @param progress Progress callback
     * @return Sync result
     */
    SyncResult exportObjects(const std::vector<std::string>& objects,
                             const SchemaOptions& options = {},
                             ProgressCallback progress = nullptr);

    /**
     * Export a single object
     * @param schema_name Schema name
     * @param object_name Object name
     * @param type Object type
     * @param options Export options
     * @return true on success
     */
    bool exportObject(const std::string& schema_name,
                      const std::string& object_name,
                      SchemaObjectType type,
                      const SchemaOptions& options = {});

    /**
     * Dry run - show what would be exported without writing
     * @param options Export options
     * @return List of objects that would be exported
     */
    std::vector<SchemaObject> dryRun(const SchemaOptions& options = {});

    //=========================================================================
    // Incremental Export
    //=========================================================================

    /**
     * Export only objects that have changed since last export
     * @param options Export options
     * @param progress Progress callback
     * @return Sync result
     */
    SyncResult exportChanged(const SchemaOptions& options = {},
                             ProgressCallback progress = nullptr);

    /**
     * Get list of objects that have changed since last export
     * @return List of changed objects
     */
    std::vector<SchemaObject> getChangedObjects();

    /**
     * Mark an object as exported (update tracking)
     * @param object Object that was exported
     */
    void markExported(const SchemaObject& object);

    //=========================================================================
    // Object Enumeration
    //=========================================================================

    /**
     * List all exportable objects
     * @param options Filter options
     * @return List of schema objects
     */
    std::vector<SchemaObject> listObjects(const SchemaOptions& options = {});

    /**
     * List objects of a specific type
     * @param type Object type
     * @param schema_name Optional schema filter
     * @return List of schema objects
     */
    std::vector<SchemaObject> listObjectsByType(SchemaObjectType type,
                                                 const std::string& schema_name = "");

    /**
     * Get a specific object
     * @param schema_name Schema name
     * @param object_name Object name
     * @param type Object type
     * @return Object info or nullopt
     */
    std::optional<SchemaObject> getObject(const std::string& schema_name,
                                           const std::string& object_name,
                                           SchemaObjectType type);

    //=========================================================================
    // DDL Generation
    //=========================================================================

    /**
     * Generate DDL for a table
     * @param schema_name Schema name
     * @param table_name Table name
     * @param options Export options
     * @return DDL string
     */
    std::string generateTableDDL(const std::string& schema_name,
                                  const std::string& table_name,
                                  const SchemaOptions& options = {});

    /**
     * Generate DDL for a view
     * @param schema_name Schema name
     * @param view_name View name
     * @return DDL string
     */
    std::string generateViewDDL(const std::string& schema_name,
                                 const std::string& view_name);

    /**
     * Generate DDL for an index
     * @param schema_name Schema name
     * @param index_name Index name
     * @return DDL string
     */
    std::string generateIndexDDL(const std::string& schema_name,
                                  const std::string& index_name);

    /**
     * Generate DDL for a function
     * @param schema_name Schema name
     * @param function_name Function name
     * @return DDL string
     */
    std::string generateFunctionDDL(const std::string& schema_name,
                                     const std::string& function_name);

    /**
     * Generate DDL for a trigger
     * @param schema_name Schema name
     * @param trigger_name Trigger name
     * @return DDL string
     */
    std::string generateTriggerDDL(const std::string& schema_name,
                                    const std::string& trigger_name);

    /**
     * Generate DDL for a domain
     * @param schema_name Schema name
     * @param domain_name Domain name
     * @return DDL string
     */
    std::string generateDomainDDL(const std::string& schema_name,
                                   const std::string& domain_name);

    /**
     * Generate DDL for a sequence
     * @param schema_name Schema name
     * @param sequence_name Sequence name
     * @return DDL string
     */
    std::string generateSequenceDDL(const std::string& schema_name,
                                     const std::string& sequence_name);

    /**
     * Generate DDL for a user-defined type
     * @param schema_name Schema name
     * @param type_name Type name
     * @return DDL string
     */
    std::string generateTypeDDL(const std::string& schema_name,
                                 const std::string& type_name);

    //=========================================================================
    // File Path Generation
    //=========================================================================

    /**
     * Get file path for an object
     * @param object Schema object
     * @param base_path Base directory path
     * @return Full file path
     */
    std::string getFilePath(const SchemaObject& object,
                            const std::string& base_path = "schema");

    /**
     * Get file name for an object
     * @param object Schema object
     * @return File name (without path)
     */
    std::string getFileName(const SchemaObject& object);

    //=========================================================================
    // Configuration
    //=========================================================================

    /**
     * Set default export options
     */
    void setDefaultOptions(const SchemaOptions& options);

    /**
     * Get default export options
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
    LogCallback log_callback_;
    std::string last_error_;

    void setError(const std::string& error);
    void log(const std::string& level, const std::string& message);

    std::string formatFileHeader(const SchemaObject& object);
    std::string formatGrants(const std::vector<std::string>& grants);
    std::string formatComments(const std::string& schema_name,
                               const std::string& object_name,
                               SchemaObjectType type);
    std::string calculateChecksum(const std::string& content);
    bool shouldExport(const std::string& schema_name,
                      const std::string& object_name,
                      const SchemaOptions& options);
    bool writeObjectFile(const SchemaObject& object, const std::string& content);
};

} // namespace git
} // namespace scratchbird

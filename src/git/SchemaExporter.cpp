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
 * Schema Exporter Implementation
 * Copyright (c) 2025 ScratchBird Project
 */

#include "scratchbird/git/SchemaExporter.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <openssl/sha.h>

namespace scratchbird {
namespace git {

//=============================================================================
// Implementation Details
//=============================================================================

struct SchemaExporter::Impl {
    // Track last export checksums for incremental export
    std::map<std::string, std::string> exported_checksums;
};

//=============================================================================
// Constructor/Destructor
//=============================================================================

SchemaExporter::SchemaExporter(core::Catalog& catalog, GitRepository& repository)
    : impl_(std::make_unique<Impl>())
    , catalog_(catalog)
    , repository_(repository) {
    // Set reasonable defaults
    default_options_.include_grants = true;
    default_options_.include_comments = true;
    default_options_.include_defaults = true;
    default_options_.separate_indexes = true;
    default_options_.file_per_object = true;
}

SchemaExporter::~SchemaExporter() = default;

//=============================================================================
// Export Operations
//=============================================================================

SyncResult SchemaExporter::exportAll(const SchemaOptions& options,
                                      ProgressCallback progress) {
    SyncResult result;
    result.direction = SyncDirection::EXPORT;
    result.timestamp = std::chrono::system_clock::now();

    try {
        auto objects = listObjects(options);
        int total = static_cast<int>(objects.size());
        int current = 0;

        for (const auto& object : objects) {
            if (progress) {
                progress(current, total, "Exporting " + object.full_name);
            }

            std::string content;
            switch (object.type) {
                case SchemaObjectType::TABLE:
                    content = generateTableDDL(object.schema_name, object.object_name, options);
                    break;
                case SchemaObjectType::VIEW:
                    content = generateViewDDL(object.schema_name, object.object_name);
                    break;
                case SchemaObjectType::INDEX:
                    content = generateIndexDDL(object.schema_name, object.object_name);
                    break;
                case SchemaObjectType::FUNCTION:
                    content = generateFunctionDDL(object.schema_name, object.object_name);
                    break;
                case SchemaObjectType::TRIGGER:
                    content = generateTriggerDDL(object.schema_name, object.object_name);
                    break;
                case SchemaObjectType::DOMAIN:
                    content = generateDomainDDL(object.schema_name, object.object_name);
                    break;
                case SchemaObjectType::SEQUENCE:
                    content = generateSequenceDDL(object.schema_name, object.object_name);
                    break;
                case SchemaObjectType::TYPE:
                    content = generateTypeDDL(object.schema_name, object.object_name);
                    break;
                default:
                    continue;
            }

            if (!content.empty()) {
                // Add file header
                SchemaObject obj_with_def = object;
                obj_with_def.definition = content;
                obj_with_def.checksum = calculateChecksum(content);

                std::string full_content = formatFileHeader(obj_with_def) + "\n" + content;

                // Add grants if enabled
                if (options.include_grants && !object.grants.empty()) {
                    full_content += "\n" + formatGrants(object.grants);
                }

                // Write to repository
                if (writeObjectFile(obj_with_def, full_content)) {
                    result.objects_exported++;
                    result.affected_objects.push_back(object.full_name);
                    markExported(obj_with_def);
                }
            }

            current++;
        }

        if (progress) {
            progress(total, total, "Export complete");
        }

        result.success = true;
        log("INFO", "Exported " + std::to_string(result.objects_exported) + " objects");

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        setError(e.what());
    }

    return result;
}

SyncResult SchemaExporter::exportSchema(const std::string& schema_name,
                                         const SchemaOptions& options,
                                         ProgressCallback progress) {
    SchemaOptions filtered_options = options;
    filtered_options.include_schemas = {schema_name};
    return exportAll(filtered_options, progress);
}

SyncResult SchemaExporter::exportObjects(const std::vector<std::string>& objects,
                                          const SchemaOptions& options,
                                          ProgressCallback progress) {
    SyncResult result;
    result.direction = SyncDirection::EXPORT;
    result.timestamp = std::chrono::system_clock::now();

    int total = static_cast<int>(objects.size());
    int current = 0;

    for (const auto& object_name : objects) {
        if (progress) {
            progress(current, total, "Exporting " + object_name);
        }

        // Parse schema.object format
        std::string schema, name;
        auto dot_pos = object_name.find('.');
        if (dot_pos != std::string::npos) {
            schema = object_name.substr(0, dot_pos);
            name = object_name.substr(dot_pos + 1);
        } else {
            schema = "public";
            name = object_name;
        }

        // Try each object type
        bool exported = false;
        for (auto type : {SchemaObjectType::TABLE, SchemaObjectType::VIEW,
                          SchemaObjectType::FUNCTION, SchemaObjectType::TRIGGER,
                          SchemaObjectType::INDEX, SchemaObjectType::SEQUENCE,
                          SchemaObjectType::DOMAIN, SchemaObjectType::TYPE}) {
            if (exportObject(schema, name, type, options)) {
                result.objects_exported++;
                result.affected_objects.push_back(object_name);
                exported = true;
                break;
            }
        }

        if (!exported) {
            log("WARN", "Object not found: " + object_name);
        }

        current++;
    }

    if (progress) {
        progress(total, total, "Export complete");
    }

    result.success = true;
    return result;
}

bool SchemaExporter::exportObject(const std::string& schema_name,
                                   const std::string& object_name,
                                   SchemaObjectType type,
                                   const SchemaOptions& options) {
    std::string content;

    switch (type) {
        case SchemaObjectType::TABLE:
            content = generateTableDDL(schema_name, object_name, options);
            break;
        case SchemaObjectType::VIEW:
            content = generateViewDDL(schema_name, object_name);
            break;
        case SchemaObjectType::INDEX:
            content = generateIndexDDL(schema_name, object_name);
            break;
        case SchemaObjectType::FUNCTION:
            content = generateFunctionDDL(schema_name, object_name);
            break;
        case SchemaObjectType::TRIGGER:
            content = generateTriggerDDL(schema_name, object_name);
            break;
        case SchemaObjectType::DOMAIN:
            content = generateDomainDDL(schema_name, object_name);
            break;
        case SchemaObjectType::SEQUENCE:
            content = generateSequenceDDL(schema_name, object_name);
            break;
        case SchemaObjectType::TYPE:
            content = generateTypeDDL(schema_name, object_name);
            break;
        default:
            return false;
    }

    if (content.empty()) {
        return false;
    }

    SchemaObject object;
    object.type = type;
    object.schema_name = schema_name;
    object.object_name = object_name;
    object.full_name = schema_name + "." + object_name;
    object.definition = content;
    object.checksum = calculateChecksum(content);

    std::string full_content = formatFileHeader(object) + "\n" + content;

    return writeObjectFile(object, full_content);
}

std::vector<SchemaObject> SchemaExporter::dryRun(const SchemaOptions& options) {
    return listObjects(options);
}

//=============================================================================
// Incremental Export
//=============================================================================

SyncResult SchemaExporter::exportChanged(const SchemaOptions& options,
                                          ProgressCallback progress) {
    auto changed = getChangedObjects();

    SyncResult result;
    result.direction = SyncDirection::EXPORT;
    result.timestamp = std::chrono::system_clock::now();

    int total = static_cast<int>(changed.size());
    int current = 0;

    for (const auto& object : changed) {
        if (progress) {
            progress(current, total, "Exporting " + object.full_name);
        }

        if (exportObject(object.schema_name, object.object_name,
                         object.type, options)) {
            result.objects_exported++;
            result.affected_objects.push_back(object.full_name);
        }

        current++;
    }

    if (progress) {
        progress(total, total, "Export complete");
    }

    result.success = true;
    return result;
}

std::vector<SchemaObject> SchemaExporter::getChangedObjects() {
    std::vector<SchemaObject> changed;
    auto all_objects = listObjects(default_options_);

    for (auto& object : all_objects) {
        std::string key = object.full_name;
        auto it = impl_->exported_checksums.find(key);

        if (it == impl_->exported_checksums.end() ||
            it->second != object.checksum) {
            changed.push_back(object);
        }
    }

    return changed;
}

void SchemaExporter::markExported(const SchemaObject& object) {
    impl_->exported_checksums[object.full_name] = object.checksum;
}

//=============================================================================
// Object Enumeration
//=============================================================================

std::vector<SchemaObject> SchemaExporter::listObjects(const SchemaOptions& options) {
    std::vector<SchemaObject> objects;

    // In production, this would query the catalog
    // For now, return placeholder implementation

    // Query tables
    auto tables = listObjectsByType(SchemaObjectType::TABLE);
    for (auto& table : tables) {
        if (shouldExport(table.schema_name, table.object_name, options)) {
            objects.push_back(table);
        }
    }

    // Query views
    auto views = listObjectsByType(SchemaObjectType::VIEW);
    for (auto& view : views) {
        if (shouldExport(view.schema_name, view.object_name, options)) {
            objects.push_back(view);
        }
    }

    // Query functions
    auto functions = listObjectsByType(SchemaObjectType::FUNCTION);
    for (auto& func : functions) {
        if (shouldExport(func.schema_name, func.object_name, options)) {
            objects.push_back(func);
        }
    }

    // Query indexes (if separate)
    if (options.separate_indexes) {
        auto indexes = listObjectsByType(SchemaObjectType::INDEX);
        for (auto& idx : indexes) {
            if (shouldExport(idx.schema_name, idx.object_name, options)) {
                objects.push_back(idx);
            }
        }
    }

    // Query sequences
    auto sequences = listObjectsByType(SchemaObjectType::SEQUENCE);
    for (auto& seq : sequences) {
        if (shouldExport(seq.schema_name, seq.object_name, options)) {
            objects.push_back(seq);
        }
    }

    // Query triggers
    auto triggers = listObjectsByType(SchemaObjectType::TRIGGER);
    for (auto& trigger : triggers) {
        if (shouldExport(trigger.schema_name, trigger.object_name, options)) {
            objects.push_back(trigger);
        }
    }

    // Query domains
    auto domains = listObjectsByType(SchemaObjectType::DOMAIN);
    for (auto& domain : domains) {
        if (shouldExport(domain.schema_name, domain.object_name, options)) {
            objects.push_back(domain);
        }
    }

    // Query types
    auto types = listObjectsByType(SchemaObjectType::TYPE);
    for (auto& type : types) {
        if (shouldExport(type.schema_name, type.object_name, options)) {
            objects.push_back(type);
        }
    }

    return objects;
}

std::vector<SchemaObject> SchemaExporter::listObjectsByType(SchemaObjectType type,
                                                             const std::string& schema_name) {
    std::vector<SchemaObject> objects;

    // In production, query the catalog based on type
    // This is a placeholder that would be filled with actual catalog queries

    // The actual implementation would look something like:
    // auto entries = catalog_.getObjects(type, schema_name);
    // for (const auto& entry : entries) {
    //     SchemaObject obj;
    //     obj.type = type;
    //     obj.schema_name = entry.schema;
    //     obj.object_name = entry.name;
    //     obj.full_name = entry.schema + "." + entry.name;
    //     objects.push_back(obj);
    // }

    return objects;
}

std::optional<SchemaObject> SchemaExporter::getObject(const std::string& schema_name,
                                                       const std::string& object_name,
                                                       SchemaObjectType type) {
    // In production, query catalog for specific object
    return std::nullopt;
}

//=============================================================================
// DDL Generation
//=============================================================================

std::string SchemaExporter::generateTableDDL(const std::string& schema_name,
                                              const std::string& table_name,
                                              const SchemaOptions& options) {
    std::stringstream ddl;

    // In production, this would query the catalog for table definition
    // and generate complete CREATE TABLE DDL

    // Placeholder structure:
    ddl << "CREATE TABLE " << schema_name << "." << table_name << " (\n";
    ddl << "    -- Columns would be generated from catalog\n";
    ddl << ");\n";

    return ddl.str();
}

std::string SchemaExporter::generateViewDDL(const std::string& schema_name,
                                             const std::string& view_name) {
    std::stringstream ddl;

    // In production, query catalog for view definition
    ddl << "CREATE VIEW " << schema_name << "." << view_name << " AS\n";
    ddl << "-- View definition would be from catalog\n";
    ddl << "SELECT 1;\n";

    return ddl.str();
}

std::string SchemaExporter::generateIndexDDL(const std::string& schema_name,
                                              const std::string& index_name) {
    std::stringstream ddl;

    // In production, query catalog for index definition
    ddl << "CREATE INDEX " << index_name << " ON " << schema_name << ".table_name (column);\n";

    return ddl.str();
}

std::string SchemaExporter::generateFunctionDDL(const std::string& schema_name,
                                                 const std::string& function_name) {
    std::stringstream ddl;

    // In production, query catalog for function definition
    ddl << "CREATE FUNCTION " << schema_name << "." << function_name << "()\n";
    ddl << "RETURNS INTEGER\n";
    ddl << "AS\n";
    ddl << "BEGIN\n";
    ddl << "    -- Function body from catalog\n";
    ddl << "    RETURN 0;\n";
    ddl << "END;\n";

    return ddl.str();
}

std::string SchemaExporter::generateTriggerDDL(const std::string& schema_name,
                                                const std::string& trigger_name) {
    std::stringstream ddl;

    // In production, query catalog for trigger definition
    ddl << "CREATE TRIGGER " << trigger_name << "\n";
    ddl << "    BEFORE INSERT ON " << schema_name << ".table_name\n";
    ddl << "    FOR EACH ROW\n";
    ddl << "BEGIN\n";
    ddl << "    -- Trigger body from catalog\n";
    ddl << "END;\n";

    return ddl.str();
}

std::string SchemaExporter::generateDomainDDL(const std::string& schema_name,
                                               const std::string& domain_name) {
    std::stringstream ddl;

    // In production, query catalog for domain definition
    ddl << "CREATE DOMAIN " << schema_name << "." << domain_name << " AS VARCHAR(255);\n";

    return ddl.str();
}

std::string SchemaExporter::generateSequenceDDL(const std::string& schema_name,
                                                 const std::string& sequence_name) {
    std::stringstream ddl;

    // In production, query catalog for sequence definition
    ddl << "CREATE SEQUENCE " << schema_name << "." << sequence_name << "\n";
    ddl << "    START WITH 1\n";
    ddl << "    INCREMENT BY 1;\n";

    return ddl.str();
}

std::string SchemaExporter::generateTypeDDL(const std::string& schema_name,
                                             const std::string& type_name) {
    std::stringstream ddl;

    // In production, query catalog for type definition
    ddl << "CREATE TYPE " << schema_name << "." << type_name << " AS (\n";
    ddl << "    -- Type members from catalog\n";
    ddl << ");\n";

    return ddl.str();
}

//=============================================================================
// File Path Generation
//=============================================================================

std::string SchemaExporter::getFilePath(const SchemaObject& object,
                                         const std::string& base_path) {
    std::string dir = base_path + "/" + getDirectoryForType(object.type);
    return dir + "/" + getFileName(object);
}

std::string SchemaExporter::getFileName(const SchemaObject& object) {
    return object.schema_name + "." + object.object_name + ".sql";
}

//=============================================================================
// Configuration
//=============================================================================

void SchemaExporter::setDefaultOptions(const SchemaOptions& options) {
    default_options_ = options;
}

const SchemaOptions& SchemaExporter::getDefaultOptions() const {
    return default_options_;
}

void SchemaExporter::setLogCallback(LogCallback callback) {
    log_callback_ = callback;
}

std::string SchemaExporter::getLastError() const {
    return last_error_;
}

//=============================================================================
// Private Methods
//=============================================================================

void SchemaExporter::setError(const std::string& error) {
    last_error_ = error;
    log("ERROR", error);
}

void SchemaExporter::log(const std::string& level, const std::string& message) {
    if (log_callback_) {
        log_callback_(level, "[SchemaExporter] " + message);
    }
}

std::string SchemaExporter::formatFileHeader(const SchemaObject& object) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream header;
    header << "-- ScratchBird Schema Export\n";
    header << "-- Object: " << toString(object.type) << " " << object.full_name << "\n";
    header << "-- Exported: " << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ") << "\n";
    header << "-- Checksum: sha256:" << object.checksum << "\n";

    return header.str();
}

std::string SchemaExporter::formatGrants(const std::vector<std::string>& grants) {
    std::stringstream result;
    result << "\n-- Grants\n";
    for (const auto& grant : grants) {
        result << grant << "\n";
    }
    return result.str();
}

std::string SchemaExporter::formatComments(const std::string& schema_name,
                                            const std::string& object_name,
                                            SchemaObjectType type) {
    // In production, query catalog for comments
    return "";
}

std::string SchemaExporter::calculateChecksum(const std::string& content) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(content.c_str()),
           content.size(), hash);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(hash[i]);
    }
    return ss.str();
}

bool SchemaExporter::shouldExport(const std::string& schema_name,
                                   const std::string& object_name,
                                   const SchemaOptions& options) {
    // Check include schemas
    if (!options.include_schemas.empty()) {
        bool found = false;
        for (const auto& s : options.include_schemas) {
            if (s == schema_name) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    // Check exclude schemas
    for (const auto& s : options.exclude_schemas) {
        if (s == schema_name) return false;
    }

    // Check exclude tables (with glob pattern support)
    for (const auto& pattern : options.exclude_tables) {
        // Simple wildcard matching
        if (pattern.back() == '*') {
            std::string prefix = pattern.substr(0, pattern.size() - 1);
            if (object_name.find(prefix) == 0) return false;
        } else if (pattern == object_name) {
            return false;
        }
    }

    return true;
}

bool SchemaExporter::writeObjectFile(const SchemaObject& object,
                                      const std::string& content) {
    std::string path = getFilePath(object, default_options_.file_per_object
                                   ? "schema" : "schema");
    return repository_.writeFile(path, content);
}

} // namespace git
} // namespace scratchbird

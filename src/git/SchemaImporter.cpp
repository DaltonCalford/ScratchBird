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
 * Schema Importer Implementation
 * Copyright (c) 2025 ScratchBird Project
 */

#include "scratchbird/git/SchemaImporter.h"
#include <sstream>
#include <algorithm>
#include <regex>
#include <set>

namespace scratchbird {
namespace git {

//=============================================================================
// Implementation Details
//=============================================================================

struct SchemaImporter::Impl {
    // Objects parsed from Git repository
    std::vector<SchemaObject> git_objects;
    // Objects from database
    std::vector<SchemaObject> db_objects;
    // Rollback scripts for applied changes
    std::vector<std::string> rollback_scripts;
};

//=============================================================================
// Constructor/Destructor
//=============================================================================

SchemaImporter::SchemaImporter(core::Catalog& catalog, GitRepository& repository)
    : impl_(std::make_unique<Impl>())
    , catalog_(catalog)
    , repository_(repository) {
}

SchemaImporter::~SchemaImporter() = default;

//=============================================================================
// Import Operations
//=============================================================================

SyncResult SchemaImporter::importAll(const SchemaOptions& options,
                                      ConflictCallback conflict_handler,
                                      ProgressCallback progress) {
    SyncResult result;
    result.direction = SyncDirection::IMPORT;
    result.timestamp = std::chrono::system_clock::now();

    try {
        // Parse objects from Git
        auto git_objects = parseObjects(options);

        // Detect conflicts
        auto conflicts = detectConflicts(options);
        if (!conflicts.empty()) {
            if (!conflict_handler) {
                // Use default strategy
                for (auto& conflict : conflicts) {
                    conflict.resolution = getStrategyForType(conflict.object_type);
                    if (conflict.resolution == ConflictStrategy::FAIL) {
                        result.success = false;
                        result.error_message = "Unresolved conflicts detected";
                        result.conflicts = conflicts;
                        return result;
                    }
                }
            } else {
                // Let callback handle each conflict
                for (auto& conflict : conflicts) {
                    conflict.resolution = conflict_handler(conflict);
                    if (conflict.resolution == ConflictStrategy::FAIL) {
                        result.success = false;
                        result.error_message = "Import aborted due to conflict";
                        result.conflicts = conflicts;
                        return result;
                    }
                }
            }
            result.conflicts = conflicts;
        }

        // Sort by dependencies
        auto sorted = sortByDependencies(git_objects);

        int total = static_cast<int>(sorted.size());
        int current = 0;

        for (const auto& object : sorted) {
            if (progress) {
                progress(current, total, "Importing " + object.full_name);
            }

            if (applyObject(object)) {
                result.objects_imported++;
                result.affected_objects.push_back(object.full_name);
            }

            current++;
        }

        if (progress) {
            progress(total, total, "Import complete");
        }

        result.success = true;
        log("INFO", "Imported " + std::to_string(result.objects_imported) + " objects");

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        setError(e.what());
    }

    return result;
}

SyncResult SchemaImporter::importSchema(const std::string& schema_name,
                                         const SchemaOptions& options,
                                         ConflictCallback conflict_handler,
                                         ProgressCallback progress) {
    SchemaOptions filtered = options;
    filtered.include_schemas = {schema_name};
    return importAll(filtered, conflict_handler, progress);
}

SyncResult SchemaImporter::importObjects(const std::vector<std::string>& objects,
                                          const SchemaOptions& options,
                                          ConflictCallback conflict_handler,
                                          ProgressCallback progress) {
    SyncResult result;
    result.direction = SyncDirection::IMPORT;
    result.timestamp = std::chrono::system_clock::now();

    int total = static_cast<int>(objects.size());
    int current = 0;

    for (const auto& object_name : objects) {
        if (progress) {
            progress(current, total, "Importing " + object_name);
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

        // Find and parse the file from Git
        for (auto type : {SchemaObjectType::TABLE, SchemaObjectType::VIEW,
                          SchemaObjectType::FUNCTION, SchemaObjectType::TRIGGER,
                          SchemaObjectType::INDEX, SchemaObjectType::SEQUENCE,
                          SchemaObjectType::DOMAIN, SchemaObjectType::TYPE}) {

            std::string path = "schema/" + std::string(getDirectoryForType(type)) +
                              "/" + schema + "." + name + ".sql";

            auto content = repository_.readFile(path);
            if (content.has_value()) {
                auto obj = parseFile(path);
                if (obj.has_value() && applyObject(obj.value())) {
                    result.objects_imported++;
                    result.affected_objects.push_back(object_name);
                    break;
                }
            }
        }

        current++;
    }

    if (progress) {
        progress(total, total, "Import complete");
    }

    result.success = true;
    return result;
}

SyncResult SchemaImporter::importFromBranch(const std::string& branch_name,
                                             const SchemaOptions& options,
                                             ConflictCallback conflict_handler,
                                             ProgressCallback progress) {
    // Checkout the branch first
    std::string current_branch = repository_.getCurrentBranch();

    if (!repository_.checkout(branch_name)) {
        SyncResult result;
        result.success = false;
        result.error_message = "Failed to checkout branch: " + branch_name;
        return result;
    }

    // Import from that branch
    auto result = importAll(options, conflict_handler, progress);

    // Return to original branch
    repository_.checkout(current_branch);

    return result;
}

std::vector<SchemaDiff> SchemaImporter::dryRun(const SchemaOptions& options) {
    return getDiff(options);
}

//=============================================================================
// Validation
//=============================================================================

std::vector<std::string> SchemaImporter::validate(const SchemaOptions& options) {
    std::vector<std::string> errors;

    auto objects = parseObjects(options);
    for (const auto& object : objects) {
        std::string error = validateFile("schema/" +
            std::string(getDirectoryForType(object.type)) + "/" +
            object.schema_name + "." + object.object_name + ".sql");
        if (!error.empty()) {
            errors.push_back(object.full_name + ": " + error);
        }
    }

    // Check dependencies
    auto dep_errors = checkDependencies();
    errors.insert(errors.end(), dep_errors.begin(), dep_errors.end());

    return errors;
}

std::string SchemaImporter::validateFile(const std::string& file_path) {
    auto content = repository_.readFile(file_path);
    if (!content.has_value()) {
        return "File not found";
    }

    // Basic syntax validation - in production would use SQL parser
    if (content->empty()) {
        return "Empty file";
    }

    // Check for required header
    if (content->find("-- ScratchBird Schema Export") == std::string::npos) {
        // Allow but warn
        log("WARN", "File missing ScratchBird header: " + file_path);
    }

    return "";  // No error
}

std::vector<std::string> SchemaImporter::checkDependencies() {
    std::vector<std::string> errors;

    auto objects = parseObjects(default_options_);
    auto sorted = sortByDependencies(objects);

    // Check for circular dependencies (would show up as remaining unsorted)
    if (sorted.size() != objects.size()) {
        errors.push_back("Circular dependencies detected in schema");
    }

    return errors;
}

//=============================================================================
// Conflict Detection and Resolution
//=============================================================================

std::vector<SchemaConflict> SchemaImporter::detectConflicts(const SchemaOptions& options) {
    std::vector<SchemaConflict> conflicts;

    auto git_objects = listGitObjects(options);
    auto db_objects = listDatabaseObjects(options);

    // Check each Git object against database
    for (const auto& git_obj : git_objects) {
        for (const auto& db_obj : db_objects) {
            if (git_obj.full_name == db_obj.full_name &&
                git_obj.type == db_obj.type) {

                // Objects exist in both - check for differences
                if (git_obj.checksum != db_obj.checksum) {
                    SchemaConflict conflict;
                    conflict.object_name = git_obj.full_name;
                    conflict.object_type = git_obj.type;
                    conflict.conflict_type = "MODIFIED";
                    conflict.local_value = db_obj.definition;
                    conflict.git_value = git_obj.definition;
                    conflicts.push_back(conflict);
                }
                break;
            }
        }
    }

    pending_conflicts_ = conflicts;
    return conflicts;
}

void SchemaImporter::setConflictStrategy(SchemaObjectType type,
                                          ConflictStrategy strategy) {
    conflict_strategies_[type] = strategy;
}

void SchemaImporter::setDefaultConflictStrategy(ConflictStrategy strategy) {
    default_conflict_strategy_ = strategy;
}

bool SchemaImporter::resolveConflict(SchemaConflict& conflict,
                                      ConflictStrategy strategy,
                                      const std::string& custom_resolution) {
    conflict.resolution = strategy;

    switch (strategy) {
        case ConflictStrategy::GIT_WINS:
            conflict.resolved_value = conflict.git_value;
            break;
        case ConflictStrategy::LOCAL_WINS:
            conflict.resolved_value = conflict.local_value;
            break;
        case ConflictStrategy::MERGE:
            // Attempt automatic merge - for now just use Git version
            conflict.resolved_value = conflict.git_value;
            break;
        case ConflictStrategy::PROMPT:
            if (custom_resolution.empty()) {
                return false;  // Need custom resolution
            }
            conflict.resolved_value = custom_resolution;
            break;
        case ConflictStrategy::FAIL:
            return false;
    }

    conflict.resolved = true;

    // Update pending conflicts
    for (auto& pending : pending_conflicts_) {
        if (pending.object_name == conflict.object_name) {
            pending = conflict;
            break;
        }
    }

    return true;
}

std::vector<SchemaConflict> SchemaImporter::getUnresolvedConflicts() const {
    std::vector<SchemaConflict> unresolved;
    for (const auto& conflict : pending_conflicts_) {
        if (!conflict.resolved) {
            unresolved.push_back(conflict);
        }
    }
    return unresolved;
}

//=============================================================================
// Diff Operations
//=============================================================================

std::vector<SchemaDiff> SchemaImporter::getDiff(const SchemaOptions& options) {
    std::vector<SchemaDiff> diffs;

    auto git_objects = listGitObjects(options);
    auto db_objects = listDatabaseObjects(options);

    // Find objects in Git but not in DB (additions)
    for (const auto& git_obj : git_objects) {
        bool found = false;
        for (const auto& db_obj : db_objects) {
            if (git_obj.full_name == db_obj.full_name &&
                git_obj.type == db_obj.type) {
                found = true;

                // Check for modifications
                if (git_obj.checksum != db_obj.checksum) {
                    SchemaDiff diff;
                    diff.type = git_obj.type;
                    diff.schema_name = git_obj.schema_name;
                    diff.object_name = git_obj.object_name;
                    diff.diff_type = DiffType::MODIFIED;
                    diff.source_definition = db_obj.definition;
                    diff.target_definition = git_obj.definition;
                    diff.unified_diff = generateUnifiedDiff(
                        db_obj.definition, git_obj.definition);
                    diffs.push_back(diff);
                }
                break;
            }
        }

        if (!found) {
            SchemaDiff diff;
            diff.type = git_obj.type;
            diff.schema_name = git_obj.schema_name;
            diff.object_name = git_obj.object_name;
            diff.diff_type = DiffType::ADDED;
            diff.target_definition = git_obj.definition;
            diffs.push_back(diff);
        }
    }

    // Find objects in DB but not in Git (removals)
    for (const auto& db_obj : db_objects) {
        bool found = false;
        for (const auto& git_obj : git_objects) {
            if (git_obj.full_name == db_obj.full_name &&
                git_obj.type == db_obj.type) {
                found = true;
                break;
            }
        }

        if (!found) {
            SchemaDiff diff;
            diff.type = db_obj.type;
            diff.schema_name = db_obj.schema_name;
            diff.object_name = db_obj.object_name;
            diff.diff_type = DiffType::REMOVED;
            diff.source_definition = db_obj.definition;
            diffs.push_back(diff);
        }
    }

    return diffs;
}

std::optional<SchemaDiff> SchemaImporter::getObjectDiff(
    const std::string& schema_name,
    const std::string& object_name,
    SchemaObjectType type) {

    auto diffs = getDiff(default_options_);
    for (const auto& diff : diffs) {
        if (diff.schema_name == schema_name &&
            diff.object_name == object_name &&
            diff.type == type) {
            return diff;
        }
    }
    return std::nullopt;
}

std::vector<SchemaDiff> SchemaImporter::getBranchDiff(const std::string& branch1,
                                                       const std::string& branch2) {
    std::vector<SchemaDiff> diffs;

    // Get files changed between branches
    auto changed_files = repository_.getChangedFiles(branch1, branch2);

    for (const auto& file : changed_files) {
        if (file.find("schema/") == 0 && file.find(".sql") != std::string::npos) {
            // Parse both versions
            auto content1 = repository_.readFile(file, branch1);
            auto content2 = repository_.readFile(file, branch2);

            SchemaDiff diff;
            // Parse object info from file path
            diff.source_definition = content1.value_or("");
            diff.target_definition = content2.value_or("");

            if (content1.has_value() && content2.has_value()) {
                diff.diff_type = DiffType::MODIFIED;
                diff.unified_diff = generateUnifiedDiff(content1.value(), content2.value());
            } else if (content2.has_value()) {
                diff.diff_type = DiffType::ADDED;
            } else {
                diff.diff_type = DiffType::REMOVED;
            }

            diffs.push_back(diff);
        }
    }

    return diffs;
}

//=============================================================================
// Object Parsing
//=============================================================================

std::vector<SchemaObject> SchemaImporter::parseObjects(const SchemaOptions& options) {
    return listGitObjects(options);
}

std::optional<SchemaObject> SchemaImporter::parseFile(const std::string& file_path) {
    auto content = repository_.readFile(file_path);
    if (!content.has_value()) {
        return std::nullopt;
    }

    SchemaObject object;

    // Parse header
    if (!parseFileHeader(content.value(), object)) {
        // Try to infer from file path
        // schema/tables/public.users.sql
        std::regex path_regex(R"(schema/(\w+)/(\w+)\.(\w+)\.sql)");
        std::smatch match;
        if (std::regex_search(file_path, match, path_regex)) {
            std::string dir = match[1];
            object.schema_name = match[2];
            object.object_name = match[3];
            object.full_name = object.schema_name + "." + object.object_name;

            // Determine type from directory
            if (dir == "tables") object.type = SchemaObjectType::TABLE;
            else if (dir == "views") object.type = SchemaObjectType::VIEW;
            else if (dir == "indexes") object.type = SchemaObjectType::INDEX;
            else if (dir == "functions") object.type = SchemaObjectType::FUNCTION;
            else if (dir == "triggers") object.type = SchemaObjectType::TRIGGER;
            else if (dir == "sequences") object.type = SchemaObjectType::SEQUENCE;
            else if (dir == "domains") object.type = SchemaObjectType::DOMAIN;
            else if (dir == "types") object.type = SchemaObjectType::TYPE;
        }
    }

    object.definition = content.value();
    return object;
}

std::optional<SchemaObject> SchemaImporter::parseDDL(const std::string& ddl) {
    SchemaObject object;

    // Simple DDL parsing - in production would use full SQL parser
    std::regex create_regex(
        R"(CREATE\s+(TABLE|VIEW|INDEX|FUNCTION|PROCEDURE|TRIGGER|SEQUENCE|DOMAIN|TYPE)\s+(\w+)\.(\w+))",
        std::regex::icase);

    std::smatch match;
    if (std::regex_search(ddl, match, create_regex)) {
        std::string type_str = match[1];
        std::transform(type_str.begin(), type_str.end(), type_str.begin(), ::toupper);

        if (type_str == "TABLE") object.type = SchemaObjectType::TABLE;
        else if (type_str == "VIEW") object.type = SchemaObjectType::VIEW;
        else if (type_str == "INDEX") object.type = SchemaObjectType::INDEX;
        else if (type_str == "FUNCTION") object.type = SchemaObjectType::FUNCTION;
        else if (type_str == "PROCEDURE") object.type = SchemaObjectType::PROCEDURE;
        else if (type_str == "TRIGGER") object.type = SchemaObjectType::TRIGGER;
        else if (type_str == "SEQUENCE") object.type = SchemaObjectType::SEQUENCE;
        else if (type_str == "DOMAIN") object.type = SchemaObjectType::DOMAIN;
        else if (type_str == "TYPE") object.type = SchemaObjectType::TYPE;

        object.schema_name = match[2];
        object.object_name = match[3];
        object.full_name = object.schema_name + "." + object.object_name;
        object.definition = ddl;

        return object;
    }

    return std::nullopt;
}

//=============================================================================
// Dependency Management
//=============================================================================

std::vector<SchemaObject> SchemaImporter::sortByDependencies(
    const std::vector<SchemaObject>& objects) {

    std::vector<SchemaObject> sorted;
    std::set<std::string> added;

    // Simple topological sort
    // First pass: add objects with no dependencies
    for (const auto& obj : objects) {
        if (obj.dependencies.empty()) {
            sorted.push_back(obj);
            added.insert(obj.full_name);
        }
    }

    // Subsequent passes: add objects whose dependencies are met
    bool progress = true;
    while (progress && sorted.size() < objects.size()) {
        progress = false;
        for (const auto& obj : objects) {
            if (added.count(obj.full_name)) continue;

            bool deps_met = true;
            for (const auto& dep : obj.dependencies) {
                if (!added.count(dep)) {
                    deps_met = false;
                    break;
                }
            }

            if (deps_met) {
                sorted.push_back(obj);
                added.insert(obj.full_name);
                progress = true;
            }
        }
    }

    return sorted;
}

std::vector<std::string> SchemaImporter::getDependencies(const SchemaObject& object) {
    // In production, would analyze DDL for references
    return object.dependencies;
}

bool SchemaImporter::canApply(const SchemaObject& object) {
    for (const auto& dep : object.dependencies) {
        // Check if dependency exists in database
        // In production, would query catalog
    }
    return true;
}

//=============================================================================
// Apply Operations
//=============================================================================

bool SchemaImporter::applyObject(const SchemaObject& object) {
    // Generate rollback script first
    // In production, would get current definition from catalog
    std::string rollback;
    switch (object.type) {
        case SchemaObjectType::TABLE:
            rollback = "DROP TABLE IF EXISTS " + object.full_name + ";";
            break;
        case SchemaObjectType::VIEW:
            rollback = "DROP VIEW IF EXISTS " + object.full_name + ";";
            break;
        case SchemaObjectType::INDEX:
            rollback = "DROP INDEX IF EXISTS " + object.object_name + ";";
            break;
        default:
            break;
    }

    if (!rollback.empty()) {
        impl_->rollback_scripts.push_back(rollback);
    }

    // Apply the DDL
    return applyDDL(object.definition);
}

bool SchemaImporter::applyDDL(const std::string& ddl) {
    // In production, would execute via catalog/session
    // For now, log the DDL
    log("INFO", "Would execute DDL:\n" + ddl.substr(0, 200) + "...");
    return true;
}

bool SchemaImporter::rollback() {
    if (impl_->rollback_scripts.empty()) {
        setError("Nothing to rollback");
        return false;
    }

    // Apply rollback scripts in reverse order
    bool success = true;
    for (auto it = impl_->rollback_scripts.rbegin();
         it != impl_->rollback_scripts.rend(); ++it) {
        if (!applyDDL(*it)) {
            success = false;
            break;
        }
    }

    if (success) {
        impl_->rollback_scripts.clear();
    }

    return success;
}

//=============================================================================
// Configuration
//=============================================================================

void SchemaImporter::setDefaultOptions(const SchemaOptions& options) {
    default_options_ = options;
}

const SchemaOptions& SchemaImporter::getDefaultOptions() const {
    return default_options_;
}

void SchemaImporter::setLogCallback(LogCallback callback) {
    log_callback_ = callback;
}

std::string SchemaImporter::getLastError() const {
    return last_error_;
}

//=============================================================================
// Private Methods
//=============================================================================

void SchemaImporter::setError(const std::string& error) {
    last_error_ = error;
    log("ERROR", error);
}

void SchemaImporter::log(const std::string& level, const std::string& message) {
    if (log_callback_) {
        log_callback_(level, "[SchemaImporter] " + message);
    }
}

bool SchemaImporter::parseFileHeader(const std::string& content, SchemaObject& object) {
    // Parse header format:
    // -- Object: TABLE public.users
    // -- Checksum: sha256:abc123...

    std::regex object_regex(R"(-- Object:\s+(\w+)\s+(\w+)\.(\w+))");
    std::regex checksum_regex(R"(-- Checksum:\s+sha256:(\w+))");

    std::smatch match;
    if (std::regex_search(content, match, object_regex)) {
        std::string type_str = match[1];

        if (type_str == "TABLE") object.type = SchemaObjectType::TABLE;
        else if (type_str == "VIEW") object.type = SchemaObjectType::VIEW;
        else if (type_str == "INDEX") object.type = SchemaObjectType::INDEX;
        else if (type_str == "FUNCTION") object.type = SchemaObjectType::FUNCTION;
        else if (type_str == "PROCEDURE") object.type = SchemaObjectType::PROCEDURE;
        else if (type_str == "TRIGGER") object.type = SchemaObjectType::TRIGGER;
        else if (type_str == "SEQUENCE") object.type = SchemaObjectType::SEQUENCE;
        else if (type_str == "DOMAIN") object.type = SchemaObjectType::DOMAIN;
        else if (type_str == "TYPE") object.type = SchemaObjectType::TYPE;
        else return false;

        object.schema_name = match[2];
        object.object_name = match[3];
        object.full_name = object.schema_name + "." + object.object_name;

        if (std::regex_search(content, match, checksum_regex)) {
            object.checksum = match[1];
        }

        return true;
    }

    return false;
}

std::string SchemaImporter::generateUnifiedDiff(const std::string& source,
                                                 const std::string& target) {
    // Simple line-by-line diff
    // In production, would use proper diff algorithm
    std::stringstream diff;

    std::istringstream source_stream(source);
    std::istringstream target_stream(target);

    std::string source_line, target_line;
    int line = 1;

    while (std::getline(source_stream, source_line) ||
           std::getline(target_stream, target_line)) {

        if (source_line != target_line) {
            if (!source_line.empty()) {
                diff << "-" << source_line << "\n";
            }
            if (!target_line.empty()) {
                diff << "+" << target_line << "\n";
            }
        } else {
            diff << " " << source_line << "\n";
        }

        source_line.clear();
        target_line.clear();
        line++;
    }

    return diff.str();
}

ConflictStrategy SchemaImporter::getStrategyForType(SchemaObjectType type) {
    auto it = conflict_strategies_.find(type);
    if (it != conflict_strategies_.end()) {
        return it->second;
    }
    return default_conflict_strategy_;
}

std::vector<SchemaObject> SchemaImporter::listGitObjects(const SchemaOptions& options) {
    std::vector<SchemaObject> objects;

    // List all .sql files in schema directory
    auto files = repository_.listFiles("schema", true);

    for (const auto& file : files) {
        if (file.find(".sql") != std::string::npos) {
            auto obj = parseFile(file);
            if (obj.has_value()) {
                objects.push_back(obj.value());
            }
        }
    }

    return objects;
}

std::vector<SchemaObject> SchemaImporter::listDatabaseObjects(const SchemaOptions& options) {
    // In production, would query catalog
    return {};
}

} // namespace git
} // namespace scratchbird

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
 * Git Integration Main Coordinator Implementation
 * Copyright (c) 2025 ScratchBird Project
 */

#include "scratchbird/git/GitIntegration.h"
#include <sstream>
#include <iomanip>
#include <chrono>

namespace scratchbird {
namespace git {

//=============================================================================
// Implementation Details
//=============================================================================

struct GitIntegration::Impl {
    std::string config_path;
};

//=============================================================================
// Constructor/Destructor
//=============================================================================

GitIntegration::GitIntegration(core::Catalog& catalog)
    : impl_(std::make_unique<Impl>())
    , catalog_(catalog)
    , config_parser_(std::make_unique<GitConfigParser>()) {
}

GitIntegration::~GitIntegration() {
    close();
}

//=============================================================================
// Initialization
//=============================================================================

bool GitIntegration::initialize(const std::string& url,
                                 const std::string& branch,
                                 const std::map<std::string, std::string>& options) {
    if (initialized_) {
        close();
    }

    config_.url = url;
    config_.branch = branch;

    // Parse options
    for (const auto& [key, value] : options) {
        if (key == "ssh_key") config_.ssh_key_path = value;
        else if (key == "auto_commit") config_.auto_commit = (value == "true");
        else if (key == "auto_push") config_.auto_push = (value == "true");
        else if (key == "local_path") config_.local_path = value;
    }

    initializeComponents();

    // Clone or open repository
    if (!config_.local_path.empty() && repository_->open()) {
        // Existing repository
        log("INFO", "Opened existing repository: " + config_.local_path);
    } else if (!url.empty()) {
        // Clone from remote
        if (!repository_->clone()) {
            setError("Failed to clone repository: " + repository_->getLastError());
            return false;
        }
    } else {
        // Initialize new local repository
        if (!repository_->init()) {
            setError("Failed to initialize repository: " + repository_->getLastError());
            return false;
        }
    }

    // Initialize migration system
    if (!migration_manager_->initialize()) {
        log("WARN", "Failed to initialize migration system");
    }

    // Initialize DDL tracking
    if (!ddl_tracker_->initialize()) {
        log("WARN", "Failed to initialize DDL tracking");
    }

    initialized_ = true;
    log("INFO", "Git integration initialized");
    return true;
}

bool GitIntegration::initializeFromConfig(const std::string& config_path) {
    impl_->config_path = config_path;

    if (!config_parser_->parseFile(config_path)) {
        setError("Failed to parse config: " + config_parser_->getLastError());
        return false;
    }

    config_ = config_parser_->getGitConfig();
    return initialize(config_.url, config_.branch, {});
}

bool GitIntegration::isInitialized() const {
    return initialized_;
}

void GitIntegration::close() {
    if (repository_) {
        repository_->close();
    }
    initialized_ = false;
}

void GitIntegration::initializeComponents() {
    repository_ = std::make_unique<GitRepository>(config_);
    exporter_ = std::make_unique<SchemaExporter>(catalog_, *repository_);
    importer_ = std::make_unique<SchemaImporter>(catalog_, *repository_);
    migration_manager_ = std::make_unique<MigrationManager>(
        catalog_, *repository_, config_parser_->getMigrationConfig());
    ddl_tracker_ = std::make_unique<DDLTracker>(catalog_);

    // Set log callbacks
    if (log_callback_) {
        repository_->setLogCallback(log_callback_);
        exporter_->setLogCallback(log_callback_);
        importer_->setLogCallback(log_callback_);
        migration_manager_->setLogCallback(log_callback_);
        ddl_tracker_->setLogCallback(log_callback_);
    }
}

//=============================================================================
// Status and Info
//=============================================================================

GitStatus GitIntegration::getStatus() const {
    if (!initialized_ || !repository_) {
        GitStatus status;
        status.state = RepositoryState::NOT_INITIALIZED;
        return status;
    }
    return repository_->getStatus();
}

std::string GitIntegration::showStatus() const {
    auto status = getStatus();
    std::stringstream ss;

    ss << "Repository: " << status.url << "\n";
    ss << "Branch: " << status.branch << "\n";
    ss << "Status: ";
    switch (status.state) {
        case RepositoryState::NOT_INITIALIZED: ss << "not initialized"; break;
        case RepositoryState::DISCONNECTED: ss << "disconnected"; break;
        case RepositoryState::CONNECTED: ss << "connected"; break;
        case RepositoryState::SYNCING: ss << "syncing"; break;
        case RepositoryState::CONFLICT: ss << "conflicts detected"; break;
        case RepositoryState::ERROR: ss << "error"; break;
    }
    ss << "\n";

    if (!status.last_commit_sha.empty()) {
        ss << "Last Commit: " << status.last_commit_sha.substr(0, 7) << "\n";
    }

    ss << "Pending Changes: " << status.pending_changes << "\n";

    if (!status.staged_files.empty()) {
        ss << "Staged Files: " << status.staged_files.size() << "\n";
    }
    if (!status.modified_files.empty()) {
        ss << "Modified Files: " << status.modified_files.size() << "\n";
    }
    if (!status.untracked_files.empty()) {
        ss << "Untracked Files: " << status.untracked_files.size() << "\n";
    }

    if (status.needs_push) {
        ss << "Note: Local commits not pushed\n";
    }
    if (status.needs_pull) {
        ss << "Note: Remote has new commits\n";
    }

    return ss.str();
}

std::string GitIntegration::getCurrentBranch() const {
    return repository_ ? repository_->getCurrentBranch() : "";
}

std::string GitIntegration::getRepositoryUrl() const {
    return config_.url;
}

//=============================================================================
// Remote Operations
//=============================================================================

bool GitIntegration::pull(ProgressCallback progress) {
    if (!initialized_) {
        setError("Git integration not initialized");
        return false;
    }
    return repository_->pull(progress);
}

bool GitIntegration::push(ProgressCallback progress) {
    if (!initialized_) {
        setError("Git integration not initialized");
        return false;
    }
    return repository_->push(progress);
}

bool GitIntegration::fetch(ProgressCallback progress) {
    if (!initialized_) {
        setError("Git integration not initialized");
        return false;
    }
    return repository_->fetch(progress);
}

//=============================================================================
// Branch Operations
//=============================================================================

bool GitIntegration::checkout(const std::string& branch) {
    if (!initialized_) {
        setError("Git integration not initialized");
        return false;
    }
    return repository_->checkout(branch);
}

bool GitIntegration::createBranch(const std::string& branch, bool checkout_branch) {
    if (!initialized_) {
        setError("Git integration not initialized");
        return false;
    }
    return repository_->createBranch(branch, checkout_branch);
}

std::vector<GitBranch> GitIntegration::listBranches(bool include_remote) const {
    if (!initialized_) {
        return {};
    }
    return repository_->listBranches(include_remote);
}

//=============================================================================
// Schema Export
//=============================================================================

SyncResult GitIntegration::exportSchema(const SchemaOptions& options,
                                         const std::string& commit_message,
                                         ProgressCallback progress) {
    if (!initialized_) {
        SyncResult result;
        result.success = false;
        result.error_message = "Git integration not initialized";
        return result;
    }

    auto result = exporter_->exportAll(options, progress);

    if (result.success && !commit_message.empty()) {
        repository_->stage();
        std::string sha = repository_->commit(commit_message);
        if (!sha.empty()) {
            result.commit_after = sha;

            if (config_.auto_push) {
                repository_->push();
            }
        }
    }

    return result;
}

SyncResult GitIntegration::exportSchemaName(const std::string& schema_name,
                                             const SchemaOptions& options,
                                             const std::string& commit_message) {
    return exporter_->exportSchema(schema_name, options, nullptr);
}

SyncResult GitIntegration::exportObjects(const std::vector<std::string>& objects,
                                          const SchemaOptions& options,
                                          const std::string& commit_message) {
    return exporter_->exportObjects(objects, options, nullptr);
}

std::vector<SchemaObject> GitIntegration::exportDryRun(const SchemaOptions& options) {
    return exporter_->dryRun(options);
}

//=============================================================================
// Schema Import
//=============================================================================

SyncResult GitIntegration::importSchema(const SchemaOptions& options,
                                         ConflictCallback conflict_handler,
                                         ProgressCallback progress) {
    if (!initialized_) {
        SyncResult result;
        result.success = false;
        result.error_message = "Git integration not initialized";
        return result;
    }

    return importer_->importAll(options, conflict_handler, progress);
}

SyncResult GitIntegration::importFromBranch(const std::string& branch,
                                             const SchemaOptions& options,
                                             ConflictCallback conflict_handler) {
    return importer_->importFromBranch(branch, options, conflict_handler, nullptr);
}

std::vector<SchemaDiff> GitIntegration::importDryRun(const SchemaOptions& options) {
    return importer_->dryRun(options);
}

//=============================================================================
// Schema Diff
//=============================================================================

std::vector<SchemaDiff> GitIntegration::getDiff() {
    return importer_->getDiff(config_parser_->getSchemaOptions());
}

std::optional<SchemaDiff> GitIntegration::getObjectDiff(const std::string& schema_name,
                                                         const std::string& object_name,
                                                         SchemaObjectType type) {
    return importer_->getObjectDiff(schema_name, object_name, type);
}

std::string GitIntegration::showDiff() {
    auto diffs = getDiff();
    std::stringstream ss;

    ss << "Object                  | Database | Git     | Difference\n";
    ss << "-----------------------|----------|---------|------------\n";

    for (const auto& diff : diffs) {
        ss << std::left << std::setw(23) << (diff.schema_name + "." + diff.object_name) << " | ";

        switch (diff.diff_type) {
            case DiffType::ADDED:
                ss << "missing  | exists  | new in Git\n";
                break;
            case DiffType::REMOVED:
                ss << "exists   | missing | not in Git\n";
                break;
            case DiffType::MODIFIED:
                ss << "exists   | exists  | modified\n";
                break;
            case DiffType::UNCHANGED:
                ss << "exists   | exists  | no difference\n";
                break;
        }
    }

    return ss.str();
}

std::string GitIntegration::showObjectDiff(const std::string& schema_name,
                                            const std::string& object_name) {
    // Try to find diff for any object type
    for (auto type : {SchemaObjectType::TABLE, SchemaObjectType::VIEW,
                      SchemaObjectType::FUNCTION, SchemaObjectType::INDEX}) {
        auto diff = getObjectDiff(schema_name, object_name, type);
        if (diff.has_value()) {
            std::stringstream ss;
            ss << "--- Database: " << diff->schema_name << "." << diff->object_name << "\n";
            ss << "+++ Git: " << diff->schema_name << "." << diff->object_name << "\n";
            ss << diff->unified_diff;
            return ss.str();
        }
    }

    return "No differences found for " + schema_name + "." + object_name;
}

std::vector<SchemaDiff> GitIntegration::getBranchDiff(const std::string& branch1,
                                                       const std::string& branch2) {
    return importer_->getBranchDiff(branch1, branch2);
}

//=============================================================================
// Migration Management
//=============================================================================

Migration GitIntegration::generateMigration(const std::string& description) {
    return migration_manager_->generate(description);
}

Migration GitIntegration::generateMigrationFromDiff(const std::string& description) {
    auto diffs = getDiff();
    return migration_manager_->generateFromDiff(description, diffs);
}

std::vector<Migration> GitIntegration::getPendingMigrations() {
    return migration_manager_->getPendingMigrations();
}

std::vector<Migration> GitIntegration::getMigrationHistory() {
    return migration_manager_->getHistory();
}

std::string GitIntegration::showPendingMigrations() {
    auto pending = getPendingMigrations();
    std::stringstream ss;

    ss << "Version | Description                    | File                        | Status\n";
    ss << "--------|-------------------------------|----------------------------|--------\n";

    for (const auto& m : pending) {
        ss << std::left << std::setw(7) << m.version << " | ";
        ss << std::setw(29) << m.description.substr(0, 29) << " | ";
        ss << std::setw(26) << m.filename.substr(0, 26) << " | ";
        ss << toString(m.state) << "\n";
    }

    return ss.str();
}

std::string GitIntegration::showMigrationHistory() {
    auto history = getMigrationHistory();
    std::stringstream ss;

    ss << "Version | Description                    | Applied At          | Duration | Checksum\n";
    ss << "--------|-------------------------------|---------------------|----------|----------\n";

    for (const auto& m : history) {
        ss << std::left << std::setw(7) << m.version << " | ";
        ss << std::setw(29) << m.description.substr(0, 29) << " | ";

        if (m.applied_at.has_value()) {
            auto time_t = std::chrono::system_clock::to_time_t(m.applied_at.value());
            ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << " | ";
        } else {
            ss << std::setw(19) << "pending" << " | ";
        }

        if (m.execution_time_ms.has_value()) {
            ss << std::setw(8) << (std::to_string(m.execution_time_ms.value()) + "ms") << " | ";
        } else {
            ss << std::setw(8) << "-" << " | ";
        }

        ss << m.checksum.substr(0, 8) << "\n";
    }

    return ss.str();
}

std::vector<MigrationResult> GitIntegration::applyMigrations(ProgressCallback progress) {
    return migration_manager_->applyAll(progress);
}

MigrationResult GitIntegration::applyMigration(const std::string& version) {
    return migration_manager_->apply(version);
}

std::vector<MigrationResult> GitIntegration::applyMigrationsTo(
    const std::string& target_version,
    ProgressCallback progress) {
    return migration_manager_->applyTo(target_version, progress);
}

MigrationResult GitIntegration::rollbackMigration() {
    return migration_manager_->rollbackLast();
}

std::vector<MigrationResult> GitIntegration::rollbackMigrationsTo(
    const std::string& target_version,
    ProgressCallback progress) {
    return migration_manager_->rollbackTo(target_version, progress);
}

std::vector<std::string> GitIntegration::validateMigrations() {
    return migration_manager_->validate();
}

//=============================================================================
// Commit Operations
//=============================================================================

std::string GitIntegration::commit(const std::string& message) {
    if (!initialized_) {
        setError("Git integration not initialized");
        return "";
    }

    repository_->stage();
    std::string sha = repository_->commit(message);

    if (!sha.empty()) {
        // Link uncommitted DDL events to this commit
        auto uncommitted = ddl_tracker_->getUncommittedEvents();
        for (const auto& event : uncommitted) {
            ddl_tracker_->linkToCommit(event.event_id, sha);
        }
    }

    return sha;
}

std::vector<DDLEvent> GitIntegration::getUncommittedChanges() {
    return ddl_tracker_->getUncommittedEvents();
}

std::string GitIntegration::showUncommittedChanges() {
    auto changes = getUncommittedChanges();
    std::stringstream ss;

    ss << "Time                | User   | Type   | Object          | Statement\n";
    ss << "--------------------|--------|--------|-----------------|----------\n";

    for (const auto& event : changes) {
        auto time_t = std::chrono::system_clock::to_time_t(event.event_time);
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << " | ";
        ss << std::left << std::setw(6) << event.user_name.substr(0, 6) << " | ";
        ss << std::setw(6) << toString(event.ddl_type) << " | ";
        ss << std::setw(15) << (event.schema_name + "." + event.object_name).substr(0, 15) << " | ";
        ss << event.ddl_statement.substr(0, 40) << "...\n";
    }

    return ss.str();
}

int GitIntegration::discardChanges() {
    return ddl_tracker_->discardUncommitted();
}

bool GitIntegration::discardChange(const std::string& event_id) {
    return ddl_tracker_->deleteEvent(event_id);
}

//=============================================================================
// Conflict Resolution
//=============================================================================

std::vector<SchemaConflict> GitIntegration::getConflicts() {
    return importer_->getUnresolvedConflicts();
}

std::string GitIntegration::showConflicts() {
    auto conflicts = getConflicts();
    std::stringstream ss;

    ss << "Object         | Conflict Type      | Local                | Git\n";
    ss << "--------------|-------------------|----------------------|----\n";

    for (const auto& conflict : conflicts) {
        ss << std::left << std::setw(14) << conflict.object_name.substr(0, 14) << " | ";
        ss << std::setw(17) << conflict.conflict_type << " | ";
        ss << std::setw(20) << conflict.local_value.substr(0, 20) << " | ";
        ss << conflict.git_value.substr(0, 20) << "\n";
    }

    return ss.str();
}

bool GitIntegration::resolveConflict(const std::string& object_name,
                                      ConflictStrategy strategy,
                                      const std::string& custom_resolution) {
    auto conflicts = importer_->getUnresolvedConflicts();
    for (auto& conflict : conflicts) {
        if (conflict.object_name == object_name) {
            return importer_->resolveConflict(conflict, strategy, custom_resolution);
        }
    }
    setError("Conflict not found: " + object_name);
    return false;
}

//=============================================================================
// DDL Tracking
//=============================================================================

void GitIntegration::enableTracking() {
    ddl_tracker_->enable();
}

void GitIntegration::disableTracking() {
    ddl_tracker_->disable();
}

bool GitIntegration::isTrackingEnabled() const {
    return ddl_tracker_->isEnabled();
}

std::string GitIntegration::trackDDL(core::Session& session,
                                      DDLType ddl_type,
                                      SchemaObjectType object_type,
                                      const std::string& schema_name,
                                      const std::string& object_name,
                                      const std::string& ddl_statement) {
    return ddl_tracker_->recordDDL(session, ddl_type, object_type,
                                    schema_name, object_name, ddl_statement);
}

//=============================================================================
// Environment Support
//=============================================================================

std::string GitIntegration::getCurrentEnvironment() const {
    return current_environment_;
}

bool GitIntegration::setEnvironment(const std::string& env_name) {
    auto env = config_parser_->getEnvironment(env_name);
    if (!env.has_value()) {
        setError("Environment not found: " + env_name);
        return false;
    }

    current_environment_ = env_name;
    log("INFO", "Switched to environment: " + env_name);
    return true;
}

std::vector<std::string> GitIntegration::listEnvironments() const {
    return config_parser_->listEnvironments();
}

std::vector<MigrationResult> GitIntegration::applyToEnvironment(
    const std::string& env_name,
    ProgressCallback progress) {

    auto env = config_parser_->getEnvironment(env_name);
    if (!env.has_value()) {
        std::vector<MigrationResult> results;
        MigrationResult error;
        error.success = false;
        error.error_message = "Environment not found: " + env_name;
        results.push_back(error);
        return results;
    }

    // In production, would connect to environment's database and apply
    return applyMigrations(progress);
}

//=============================================================================
// Configuration
//=============================================================================

const GitConfig& GitIntegration::getConfig() const {
    return config_;
}

void GitIntegration::setConfig(const GitConfig& config) {
    config_ = config;
    if (repository_) {
        repository_->setConfig(config);
    }
}

GitConfigParser& GitIntegration::getConfigParser() {
    return *config_parser_;
}

bool GitIntegration::reloadConfig() {
    if (impl_->config_path.empty()) {
        setError("No config file path");
        return false;
    }

    return config_parser_->parseFile(impl_->config_path);
}

//=============================================================================
// Component Access
//=============================================================================

GitRepository& GitIntegration::getRepository() {
    return *repository_;
}

SchemaExporter& GitIntegration::getExporter() {
    return *exporter_;
}

SchemaImporter& GitIntegration::getImporter() {
    return *importer_;
}

MigrationManager& GitIntegration::getMigrationManager() {
    return *migration_manager_;
}

DDLTracker& GitIntegration::getDDLTracker() {
    return *ddl_tracker_;
}

//=============================================================================
// Error Handling
//=============================================================================

void GitIntegration::setLogCallback(LogCallback callback) {
    log_callback_ = callback;

    if (repository_) repository_->setLogCallback(callback);
    if (exporter_) exporter_->setLogCallback(callback);
    if (importer_) importer_->setLogCallback(callback);
    if (migration_manager_) migration_manager_->setLogCallback(callback);
    if (ddl_tracker_) ddl_tracker_->setLogCallback(callback);
}

std::string GitIntegration::getLastError() const {
    return last_error_;
}

void GitIntegration::setError(const std::string& error) {
    last_error_ = error;
    log("ERROR", error);
}

void GitIntegration::log(const std::string& level, const std::string& message) {
    if (log_callback_) {
        log_callback_(level, "[GitIntegration] " + message);
    }
}

} // namespace git
} // namespace scratchbird

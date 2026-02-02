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
 * Migration Manager Implementation
 * Copyright (c) 2025 ScratchBird Project
 */

#include "scratchbird/git/MigrationManager.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <chrono>
#include <openssl/sha.h>

namespace scratchbird {
namespace git {

//=============================================================================
// Implementation Details
//=============================================================================

struct MigrationManager::Impl {
    std::map<std::string, Migration> applied_migrations;
    std::vector<Migration> pending_migrations;
    int next_version_number = 1;
};

//=============================================================================
// Constructor/Destructor
//=============================================================================

MigrationManager::MigrationManager(core::Catalog& catalog,
                                   GitRepository& repository,
                                   const MigrationConfig& config)
    : impl_(std::make_unique<Impl>())
    , catalog_(catalog)
    , repository_(repository)
    , config_(config) {
}

MigrationManager::~MigrationManager() {
    if (lock_held_) {
        releaseLock();
    }
}

//=============================================================================
// Initialization
//=============================================================================

bool MigrationManager::initialize() {
    // Create migration tracking table if not exists
    std::string create_migrations = R"(
CREATE TABLE IF NOT EXISTS SYS$MIGRATIONS (
    version VARCHAR(64) PRIMARY KEY,
    description VARCHAR(256),
    script_name VARCHAR(256) NOT NULL,
    checksum VARCHAR(64) NOT NULL,
    applied_at TIMESTAMP NOT NULL,
    applied_by VARCHAR(128) NOT NULL,
    execution_time_ms INTEGER,
    success BOOLEAN NOT NULL,
    error_message TEXT,
    rolled_back_at TIMESTAMP,
    rolled_back_by VARCHAR(128)
);
)";

    std::string create_lock = R"(
CREATE TABLE IF NOT EXISTS SYS$MIGRATION_LOCK (
    lock_id INTEGER PRIMARY KEY DEFAULT 1,
    locked_at TIMESTAMP,
    locked_by VARCHAR(128),
    lock_reason VARCHAR(256),
    CONSTRAINT single_lock CHECK (lock_id = 1)
);

INSERT INTO SYS$MIGRATION_LOCK (lock_id) VALUES (1) ON CONFLICT DO NOTHING;
)";

    // In production, would execute via catalog
    log("INFO", "Migration system initialized");
    return true;
}

bool MigrationManager::isInitialized() const {
    // In production, check if tables exist
    return true;
}

std::string MigrationManager::getCurrentVersion() const {
    // In production, query SYS$MIGRATIONS for latest
    if (impl_->applied_migrations.empty()) {
        return "";
    }

    // Find highest version
    std::string max_version;
    for (const auto& [version, _] : impl_->applied_migrations) {
        if (max_version.empty() || version > max_version) {
            max_version = version;
        }
    }
    return max_version;
}

//=============================================================================
// Migration Discovery
//=============================================================================

std::vector<Migration> MigrationManager::scanMigrations() {
    std::vector<Migration> migrations;

    // List migration files
    auto files = repository_.listFiles(config_.table_name.empty()
                                       ? "migrations" : "migrations", false);

    for (const auto& file : files) {
        if (file.find(".sql") == std::string::npos) continue;

        Migration migration = parseMigrationFile(file);
        if (!migration.version.empty()) {
            migrations.push_back(migration);
        }
    }

    // Sort by version
    std::sort(migrations.begin(), migrations.end(),
              [](const Migration& a, const Migration& b) {
                  return a.version < b.version;
              });

    return migrations;
}

std::vector<Migration> MigrationManager::getPendingMigrations() {
    std::vector<Migration> pending;
    auto all = scanMigrations();

    for (auto& migration : all) {
        if (impl_->applied_migrations.find(migration.version) ==
            impl_->applied_migrations.end()) {
            migration.state = MigrationState::PENDING;
            pending.push_back(migration);
        }
    }

    return pending;
}

std::vector<Migration> MigrationManager::getAppliedMigrations() {
    std::vector<Migration> applied;
    for (const auto& [version, migration] : impl_->applied_migrations) {
        applied.push_back(migration);
    }

    std::sort(applied.begin(), applied.end(),
              [](const Migration& a, const Migration& b) {
                  return a.version < b.version;
              });

    return applied;
}

std::optional<Migration> MigrationManager::getMigration(const std::string& version) {
    auto it = impl_->applied_migrations.find(version);
    if (it != impl_->applied_migrations.end()) {
        return it->second;
    }

    // Check pending
    auto pending = getPendingMigrations();
    for (const auto& m : pending) {
        if (m.version == version) {
            return m;
        }
    }

    return std::nullopt;
}

std::vector<Migration> MigrationManager::getHistory() {
    auto applied = getAppliedMigrations();
    auto pending = getPendingMigrations();

    std::vector<Migration> all;
    all.reserve(applied.size() + pending.size());
    all.insert(all.end(), applied.begin(), applied.end());
    all.insert(all.end(), pending.begin(), pending.end());

    std::sort(all.begin(), all.end(),
              [](const Migration& a, const Migration& b) {
                  return a.version < b.version;
              });

    return all;
}

//=============================================================================
// Migration Execution
//=============================================================================

std::vector<MigrationResult> MigrationManager::applyAll(ProgressCallback progress) {
    std::vector<MigrationResult> results;

    if (!acquireLock("Applying all migrations")) {
        MigrationResult error;
        error.success = false;
        error.error_message = "Could not acquire migration lock";
        results.push_back(error);
        return results;
    }

    auto pending = getPendingMigrations();
    int total = static_cast<int>(pending.size());
    int current = 0;

    for (auto& migration : pending) {
        if (progress) {
            progress(current, total, "Applying " + migration.version);
        }

        auto result = apply(migration.version);
        results.push_back(result);

        if (!result.success) {
            break;  // Stop on first failure
        }

        current++;
    }

    if (progress) {
        progress(total, total, "Migration complete");
    }

    releaseLock();
    return results;
}

std::vector<MigrationResult> MigrationManager::applyTo(const std::string& target_version,
                                                        ProgressCallback progress) {
    std::vector<MigrationResult> results;

    auto pending = getPendingMigrations();
    int total = 0;
    for (const auto& m : pending) {
        if (m.version <= target_version) total++;
    }

    if (!acquireLock("Applying migrations to " + target_version)) {
        MigrationResult error;
        error.success = false;
        error.error_message = "Could not acquire migration lock";
        results.push_back(error);
        return results;
    }

    int current = 0;
    for (auto& migration : pending) {
        if (migration.version > target_version) break;

        if (progress) {
            progress(current, total, "Applying " + migration.version);
        }

        auto result = apply(migration.version);
        results.push_back(result);

        if (!result.success) {
            break;
        }

        current++;
    }

    if (progress) {
        progress(total, total, "Migration complete");
    }

    releaseLock();
    return results;
}

MigrationResult MigrationManager::apply(const std::string& version) {
    MigrationResult result;
    result.version = version;

    auto migration_opt = getMigration(version);
    if (!migration_opt.has_value()) {
        result.success = false;
        result.error_message = "Migration not found: " + version;
        return result;
    }

    auto& migration = migration_opt.value();

    // Check if already applied
    if (impl_->applied_migrations.count(version) > 0) {
        result.success = false;
        result.error_message = "Migration already applied: " + version;
        return result;
    }

    // Check checksum if previously applied
    if (config_.checksum_validation) {
        // In production, validate against stored checksum
    }

    // Check dependencies
    if (!dependenciesMet(version)) {
        result.success = false;
        result.error_message = "Dependencies not met for: " + version;
        return result;
    }

    result.description = migration.description;

    auto start = std::chrono::steady_clock::now();

    // Execute the up script
    // In production, would execute via session
    bool success = executeScript(migration.up_script, *(core::Session*)nullptr);

    auto end = std::chrono::steady_clock::now();
    result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    if (success) {
        result.success = true;
        migration.state = MigrationState::APPLIED;
        migration.applied_at = std::chrono::system_clock::now();
        migration.applied_by = "system";  // In production, get from session
        migration.execution_time_ms = result.execution_time_ms;
        impl_->applied_migrations[version] = migration;

        recordMigration(migration, true);
        log("INFO", "Applied migration: " + version);
    } else {
        result.success = false;
        result.error_message = last_error_;
        migration.state = MigrationState::FAILED;

        recordMigration(migration, false, last_error_);
        log("ERROR", "Migration failed: " + version);
    }

    return result;
}

MigrationResult MigrationManager::applyMigration(const Migration& migration,
                                                  core::Session& session) {
    return apply(migration.version);
}

bool MigrationManager::skip(const std::string& version, const std::string& reason) {
    auto migration_opt = getMigration(version);
    if (!migration_opt.has_value()) {
        setError("Migration not found: " + version);
        return false;
    }

    auto migration = migration_opt.value();
    migration.state = MigrationState::SKIPPED;
    migration.applied_at = std::chrono::system_clock::now();
    migration.applied_by = "system";
    impl_->applied_migrations[version] = migration;

    log("INFO", "Skipped migration: " + version + " (" + reason + ")");
    return true;
}

//=============================================================================
// Rollback Operations
//=============================================================================

MigrationResult MigrationManager::rollbackLast() {
    auto applied = getAppliedMigrations();
    if (applied.empty()) {
        MigrationResult result;
        result.success = false;
        result.error_message = "No migrations to rollback";
        return result;
    }

    return rollback(1).front();
}

std::vector<MigrationResult> MigrationManager::rollback(int count,
                                                         ProgressCallback progress) {
    std::vector<MigrationResult> results;

    auto applied = getAppliedMigrations();
    if (applied.empty()) {
        MigrationResult error;
        error.success = false;
        error.error_message = "No migrations to rollback";
        results.push_back(error);
        return results;
    }

    if (!acquireLock("Rolling back " + std::to_string(count) + " migrations")) {
        MigrationResult error;
        error.success = false;
        error.error_message = "Could not acquire migration lock";
        results.push_back(error);
        return results;
    }

    // Rollback in reverse order
    std::reverse(applied.begin(), applied.end());

    int total = std::min(count, static_cast<int>(applied.size()));
    int current = 0;

    for (int i = 0; i < total && i < static_cast<int>(applied.size()); i++) {
        auto& migration = applied[i];

        if (progress) {
            progress(current, total, "Rolling back " + migration.version);
        }

        MigrationResult result;
        result.version = migration.version;
        result.description = migration.description;

        if (!canRollback(migration.version)) {
            result.success = false;
            result.error_message = "Cannot rollback: " + migration.version;
            results.push_back(result);
            break;
        }

        auto start = std::chrono::steady_clock::now();

        bool success = executeScript(migration.down_script, *(core::Session*)nullptr);

        auto end = std::chrono::steady_clock::now();
        result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();

        if (success) {
            result.success = true;
            impl_->applied_migrations.erase(migration.version);
            recordRollback(migration.version);
            log("INFO", "Rolled back migration: " + migration.version);
        } else {
            result.success = false;
            result.error_message = last_error_;
            log("ERROR", "Rollback failed: " + migration.version);
        }

        results.push_back(result);
        if (!success) break;

        current++;
    }

    if (progress) {
        progress(total, total, "Rollback complete");
    }

    releaseLock();
    return results;
}

std::vector<MigrationResult> MigrationManager::rollbackTo(
    const std::string& target_version,
    ProgressCallback progress) {

    std::vector<MigrationResult> results;
    auto applied = getAppliedMigrations();

    // Count how many to rollback
    int count = 0;
    for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
        if (it->version <= target_version) break;
        count++;
    }

    if (count == 0) {
        return results;
    }

    return rollback(count, progress);
}

bool MigrationManager::canRollback(const std::string& version) const {
    auto it = impl_->applied_migrations.find(version);
    if (it == impl_->applied_migrations.end()) {
        return false;
    }

    // Check if there's a down script
    return !it->second.down_script.empty();
}

//=============================================================================
// Migration Generation
//=============================================================================

Migration MigrationManager::generate(const std::string& description) {
    Migration migration;
    migration.version = generateVersion();
    migration.description = description;
    migration.filename = generateFileName(migration.version, description);
    migration.state = MigrationState::PENDING;

    // Template content
    std::stringstream content;
    content << "-- Migration: " << migration.version << "\n";
    content << "-- Description: " << description << "\n";
    content << "-- Date: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n";
    content << "\n";
    content << "-- @up\n";
    content << "-- Add your migration SQL here\n";
    content << "\n";
    content << "-- @down\n";
    content << "-- Add your rollback SQL here\n";

    migration.up_script = "-- Add your migration SQL here";
    migration.down_script = "-- Add your rollback SQL here";
    migration.checksum = calculateChecksum(content.str());

    return migration;
}

Migration MigrationManager::generateFromDiff(const std::string& description,
                                              const std::vector<SchemaDiff>& diffs) {
    Migration migration = generate(description);

    std::stringstream up_script;
    std::stringstream down_script;

    for (const auto& diff : diffs) {
        switch (diff.diff_type) {
            case DiffType::ADDED:
                up_script << diff.target_definition << "\n\n";
                down_script << "DROP " << toString(diff.type) << " IF EXISTS "
                           << diff.schema_name << "." << diff.object_name << ";\n\n";
                break;

            case DiffType::REMOVED:
                up_script << "DROP " << toString(diff.type) << " IF EXISTS "
                         << diff.schema_name << "." << diff.object_name << ";\n\n";
                down_script << diff.source_definition << "\n\n";
                break;

            case DiffType::MODIFIED:
                // For modifications, need to generate ALTER statements
                // This is simplified - real implementation would analyze differences
                up_script << "-- Modified: " << diff.schema_name << "."
                         << diff.object_name << "\n";
                up_script << diff.target_definition << "\n\n";
                down_script << diff.source_definition << "\n\n";
                break;

            default:
                break;
        }
    }

    migration.up_script = up_script.str();
    migration.down_script = down_script.str();
    migration.checksum = calculateChecksum(migration.up_script + migration.down_script);

    return migration;
}

Migration MigrationManager::generateFromTemplate(const std::string& description,
                                                  const std::string& template_name,
                                                  const std::map<std::string, std::string>& parameters) {
    Migration migration = generate(description);

    // In production, load template and substitute parameters
    // For now, return basic migration
    return migration;
}

std::string MigrationManager::saveMigration(const Migration& migration) {
    std::stringstream content;
    content << "-- Migration: " << migration.version << "\n";
    content << "-- Description: " << migration.description << "\n";
    if (migration.author.has_value()) {
        content << "-- Author: " << migration.author.value() << "\n";
    }
    if (migration.date.has_value()) {
        content << "-- Date: " << migration.date.value() << "\n";
    }
    if (!migration.dependencies.empty()) {
        content << "-- Dependencies: ";
        for (size_t i = 0; i < migration.dependencies.size(); i++) {
            if (i > 0) content << ", ";
            content << migration.dependencies[i];
        }
        content << "\n";
    }
    content << "\n";
    content << "-- @up\n";
    content << migration.up_script << "\n";
    content << "\n";
    content << "-- @down\n";
    content << migration.down_script << "\n";

    std::string path = "migrations/" + migration.filename;
    if (repository_.writeFile(path, content.str())) {
        log("INFO", "Saved migration: " + path);
        return path;
    }

    setError("Failed to save migration");
    return "";
}

//=============================================================================
// Validation
//=============================================================================

std::vector<std::string> MigrationManager::validate() {
    std::vector<std::string> errors;

    auto migrations = scanMigrations();

    for (const auto& migration : migrations) {
        std::string error = validateMigration(migration.version);
        if (!error.empty()) {
            errors.push_back(migration.version + ": " + error);
        }
    }

    // Check for gaps
    auto gaps = checkVersionGaps();
    for (const auto& gap : gaps) {
        errors.push_back("Missing version: " + gap);
    }

    // Check for out-of-order
    auto ooo = checkOutOfOrder();
    for (const auto& v : ooo) {
        errors.push_back("Out of order: " + v);
    }

    return errors;
}

std::string MigrationManager::validateMigration(const std::string& version) {
    auto migration_opt = getMigration(version);
    if (!migration_opt.has_value()) {
        return "Migration not found";
    }

    auto& migration = migration_opt.value();

    // Check for @up section
    if (migration.up_script.empty()) {
        return "Missing @up section";
    }

    // Check for @down section if required
    if (config_.generate_down && migration.down_script.empty()) {
        return "Missing @down section";
    }

    return "";
}

std::vector<Migration> MigrationManager::validateChecksums() {
    std::vector<Migration> mismatched;

    for (const auto& [version, migration] : impl_->applied_migrations) {
        auto file_migration = parseMigrationFile("migrations/" + migration.filename);
        if (file_migration.checksum != migration.checksum) {
            mismatched.push_back(migration);
        }
    }

    return mismatched;
}

std::vector<std::string> MigrationManager::checkVersionGaps() {
    std::vector<std::string> gaps;
    // In production, check for missing version numbers
    return gaps;
}

std::vector<std::string> MigrationManager::checkOutOfOrder() {
    std::vector<std::string> out_of_order;
    // In production, check if any migrations were applied out of order
    return out_of_order;
}

//=============================================================================
// Locking
//=============================================================================

bool MigrationManager::acquireLock(const std::string& reason, int timeout_seconds) {
    // In production, would use database advisory locks or UPDATE with WHERE
    // For now, use simple in-memory flag
    if (lock_held_) {
        setError("Migration lock already held");
        return false;
    }

    lock_held_ = true;
    log("INFO", "Acquired migration lock: " + reason);
    return true;
}

bool MigrationManager::releaseLock() {
    if (!lock_held_) {
        return false;
    }

    lock_held_ = false;
    log("INFO", "Released migration lock");
    return true;
}

bool MigrationManager::forceReleaseLock() {
    // In production, admin-only operation to force release
    lock_held_ = false;
    log("WARN", "Force released migration lock");
    return true;
}

std::optional<std::pair<std::string, std::chrono::system_clock::time_point>>
MigrationManager::getLockInfo() const {
    if (!lock_held_) {
        return std::nullopt;
    }
    return std::make_pair("system", std::chrono::system_clock::now());
}

//=============================================================================
// Repair Operations
//=============================================================================

bool MigrationManager::baseline(const std::string& version) {
    // Mark all migrations up to version as applied without executing
    auto all = scanMigrations();

    for (auto& migration : all) {
        if (migration.version <= version) {
            migration.state = MigrationState::APPLIED;
            migration.applied_at = std::chrono::system_clock::now();
            migration.applied_by = "baseline";
            impl_->applied_migrations[migration.version] = migration;
        }
    }

    log("INFO", "Baselined migrations at: " + version);
    return true;
}

bool MigrationManager::repairChecksum(const std::string& version,
                                       const std::string& checksum) {
    auto it = impl_->applied_migrations.find(version);
    if (it == impl_->applied_migrations.end()) {
        setError("Migration not found: " + version);
        return false;
    }

    it->second.checksum = checksum;
    log("INFO", "Repaired checksum for: " + version);
    return true;
}

bool MigrationManager::removeFromHistory(const std::string& version) {
    if (impl_->applied_migrations.erase(version) > 0) {
        log("INFO", "Removed migration from history: " + version);
        return true;
    }

    setError("Migration not found: " + version);
    return false;
}

//=============================================================================
// Dependency Management
//=============================================================================

std::map<std::string, std::vector<std::string>> MigrationManager::getDependencyGraph() {
    std::map<std::string, std::vector<std::string>> graph;

    auto all = scanMigrations();
    for (const auto& migration : all) {
        graph[migration.version] = migration.dependencies;
    }

    return graph;
}

bool MigrationManager::dependenciesMet(const std::string& version) {
    auto migration_opt = getMigration(version);
    if (!migration_opt.has_value()) {
        return false;
    }

    for (const auto& dep : migration_opt->dependencies) {
        if (impl_->applied_migrations.find(dep) == impl_->applied_migrations.end()) {
            return false;
        }
    }

    return true;
}

std::vector<std::string> MigrationManager::getBlockingMigrations(
    const std::string& version) {

    std::vector<std::string> blocking;
    auto migration_opt = getMigration(version);
    if (!migration_opt.has_value()) {
        return blocking;
    }

    for (const auto& dep : migration_opt->dependencies) {
        if (impl_->applied_migrations.find(dep) == impl_->applied_migrations.end()) {
            blocking.push_back(dep);
        }
    }

    return blocking;
}

//=============================================================================
// Configuration
//=============================================================================

void MigrationManager::setConfig(const MigrationConfig& config) {
    config_ = config;
}

const MigrationConfig& MigrationManager::getConfig() const {
    return config_;
}

void MigrationManager::setLogCallback(LogCallback callback) {
    log_callback_ = callback;
}

std::string MigrationManager::getLastError() const {
    return last_error_;
}

//=============================================================================
// Private Methods
//=============================================================================

void MigrationManager::setError(const std::string& error) {
    last_error_ = error;
    log("ERROR", error);
}

void MigrationManager::log(const std::string& level, const std::string& message) {
    if (log_callback_) {
        log_callback_(level, "[MigrationManager] " + message);
    }
}

Migration MigrationManager::parseMigrationFile(const std::string& path) {
    Migration migration;

    auto content = repository_.readFile(path);
    if (!content.has_value()) {
        return migration;
    }

    // Parse filename for version
    // V001__description.sql or 20240301__description.sql
    std::regex version_regex(R"(([VR]?\d+)__(.+)\.sql)");
    std::smatch match;
    if (std::regex_search(path, match, version_regex)) {
        migration.version = match[1];
        migration.description = match[2];
        // Replace underscores with spaces
        std::replace(migration.description.begin(),
                    migration.description.end(), '_', ' ');
    }

    migration.filename = path.substr(path.rfind('/') + 1);

    // Parse header comments
    std::regex author_regex(R"(-- Author:\s*(.+))");
    std::regex date_regex(R"(-- Date:\s*(.+))");
    std::regex deps_regex(R"(-- Dependencies:\s*(.+))");

    if (std::regex_search(content.value(), match, author_regex)) {
        migration.author = match[1];
    }
    if (std::regex_search(content.value(), match, date_regex)) {
        migration.date = match[1];
    }
    if (std::regex_search(content.value(), match, deps_regex)) {
        std::string deps = match[1];
        std::istringstream ss(deps);
        std::string dep;
        while (std::getline(ss, dep, ',')) {
            // Trim whitespace
            dep.erase(0, dep.find_first_not_of(" \t"));
            dep.erase(dep.find_last_not_of(" \t") + 1);
            if (!dep.empty()) {
                migration.dependencies.push_back(dep);
            }
        }
    }

    // Parse @up and @down sections
    size_t up_pos = content->find("-- @up");
    size_t down_pos = content->find("-- @down");

    if (up_pos != std::string::npos) {
        size_t start = up_pos + 6;
        size_t end = (down_pos != std::string::npos) ? down_pos : content->size();
        migration.up_script = content->substr(start, end - start);
        // Trim
        migration.up_script.erase(0, migration.up_script.find_first_not_of(" \n\r\t"));
    }

    if (down_pos != std::string::npos) {
        size_t start = down_pos + 8;
        migration.down_script = content->substr(start);
        // Trim
        migration.down_script.erase(0, migration.down_script.find_first_not_of(" \n\r\t"));
    }

    migration.checksum = calculateChecksum(content.value());

    return migration;
}

std::string MigrationManager::generateVersion() {
    switch (config_.naming) {
        case MigrationNaming::VERSIONED: {
            // Find next version number
            auto all = scanMigrations();
            int max_version = 0;
            for (const auto& m : all) {
                if (m.version[0] == 'V') {
                    try {
                        int v = std::stoi(m.version.substr(1));
                        max_version = std::max(max_version, v);
                    } catch (...) {}
                }
            }
            std::stringstream ss;
            ss << "V" << std::setfill('0') << std::setw(3) << (max_version + 1);
            return ss.str();
        }

        case MigrationNaming::TIMESTAMP: {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t), "%Y%m%d%H%M%S");
            return ss.str();
        }

        case MigrationNaming::SEQUENTIAL: {
            auto all = scanMigrations();
            std::stringstream ss;
            ss << std::setfill('0') << std::setw(3) << (all.size() + 1);
            return ss.str();
        }
    }

    return "V001";
}

std::string MigrationManager::generateFileName(const std::string& version,
                                                const std::string& description) {
    std::string safe_desc = description;
    // Replace spaces and special chars with underscores
    for (char& c : safe_desc) {
        if (!std::isalnum(c)) c = '_';
    }
    // Collapse multiple underscores
    safe_desc.erase(
        std::unique(safe_desc.begin(), safe_desc.end(),
                   [](char a, char b) { return a == '_' && b == '_'; }),
        safe_desc.end());

    return version + "__" + safe_desc + ".sql";
}

std::string MigrationManager::calculateChecksum(const std::string& content) {
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

bool MigrationManager::executeScript(const std::string& script,
                                      core::Session& session) {
    // In production, would execute via session
    log("INFO", "Would execute script:\n" + script.substr(0, 200) + "...");
    return true;
}

bool MigrationManager::recordMigration(const Migration& migration, bool success,
                                        const std::string& error) {
    // In production, INSERT INTO SYS$MIGRATIONS
    return true;
}

bool MigrationManager::recordRollback(const std::string& version) {
    // In production, UPDATE SYS$MIGRATIONS SET rolled_back_at = ...
    return true;
}

std::vector<Migration> MigrationManager::sortByDependencies(
    const std::vector<Migration>& migrations) {

    std::vector<Migration> sorted;
    std::set<std::string> added;

    // Simple topological sort
    bool progress = true;
    while (progress && sorted.size() < migrations.size()) {
        progress = false;
        for (const auto& m : migrations) {
            if (added.count(m.version)) continue;

            bool deps_met = true;
            for (const auto& dep : m.dependencies) {
                if (!added.count(dep)) {
                    deps_met = false;
                    break;
                }
            }

            if (deps_met) {
                sorted.push_back(m);
                added.insert(m.version);
                progress = true;
            }
        }
    }

    return sorted;
}

} // namespace git
} // namespace scratchbird

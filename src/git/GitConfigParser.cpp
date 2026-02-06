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
 * Git Configuration Parser Implementation
 * Copyright (c) 2025 ScratchBird Project
 */

#include "scratchbird/git/GitConfigParser.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <cstdlib>
#include <algorithm>
#include <set>

namespace scratchbird {
namespace git {

//=============================================================================
// Key Mapping for Canonical Keys and Legacy Aliases
//=============================================================================

/**
 * Maps legacy keys to canonical keys.
 * When both are present, canonical keys take precedence.
 */
static const std::map<std::string, std::string> LEGACY_TO_CANONICAL = {
    // Repository section
    {"repository.type", "repository.repo_type"},
    {"repository.url", "repository.repo_url"},
    {"repository.branch", "repository.repo_branch"},
    {"repository.path", "repository.repo_path"},
    {"repository.mode", "repository.repo_mode"},
    {"repository.ssh_key", "repository.ssh_key_path"},
    
    // Legacy boolean flags that map to repo_mode (these are tracked as legacy keys)
    {"repository.auto_commit", "repository.repo_mode"},
    {"repository.auto_push", "repository.repo_mode"},
    {"repository.auto_pull", "repository.repo_mode"},
    
    // Git section (INI format)
    {"git.type", "repository.repo_type"},
    {"git.url", "repository.repo_url"},
    {"git.branch", "repository.repo_branch"},
    {"git.path", "repository.repo_path"},
    {"git.mode", "repository.repo_mode"},
    {"git.ssh_key", "repository.ssh_key_path"},
    {"git.sign_commits", "repository.sign_commits"},
    {"git.commit_template", "repository.commit_template"},
    {"git.gpg_key_id", "repository.gpg_key_id"},
    
    // [git.repository] section (INI format - with section prefix)
    {"git.repository.type", "repository.repo_type"},
    {"git.repository.url", "repository.repo_url"},
    {"git.repository.branch", "repository.repo_branch"},
    {"git.repository.path", "repository.repo_path"},
    {"git.repository.mode", "repository.repo_mode"},
    {"git.repository.ssh_key", "repository.ssh_key_path"},
    {"git.repository.sign_commits", "repository.sign_commits"},
    {"git.repository.commit_template", "repository.commit_template"},
    {"git.repository.gpg_key_id", "repository.gpg_key_id"},
    
    // Schema section (INI format)
    {"git.schema.directory", "schema.directory"},
    {"git.schema.include_grants", "schema.include_grants"},
    {"git.schema.include_comments", "schema.include_comments"},
    {"git.schema.include_defaults", "schema.include_defaults"},
    {"git.schema.separate_indexes", "schema.separate_indexes"},
    {"git.schema.file_per_object", "schema.file_per_object"},
    
    // Migrations section (INI format)
    {"git.migrations.directory", "migrations.directory"},
    {"git.migrations.table", "migrations.table"},
    {"git.migrations.naming", "migrations.naming"},
    {"git.migrations.generate_down", "migrations.generate_down"},
    {"git.migrations.transaction_per_file", "migrations.transaction_per_file"},
    {"git.migrations.checksum_validation", "migrations.checksum_validation"},
};

/**
 * Check if a key is a deprecated legacy key
 */
static bool isLegacyKey(const std::string& key) {
    return LEGACY_TO_CANONICAL.find(key) != LEGACY_TO_CANONICAL.end();
}

/**
 * Get canonical key for a legacy key, or return original if not legacy
 */
static std::string getCanonicalKey(const std::string& key) {
    auto it = LEGACY_TO_CANONICAL.find(key);
    if (it != LEGACY_TO_CANONICAL.end()) {
        return it->second;
    }
    return key;
}

/**
 * Normalize a key to its canonical form for consistent storage and lookup.
 * This handles both legacy keys and INI-format canonical keys.
 * Examples:
 *   - "repository.url" -> "repository.repo_url"
 *   - "git.repository.url" -> "repository.repo_url"
 *   - "git.repository.repo_url" -> "repository.repo_url"
 *   - "repository.repo_url" -> "repository.repo_url"
 */
static std::string normalizeKey(const std::string& key) {
    // First check if it's a legacy key (direct lookup)
    auto it = LEGACY_TO_CANONICAL.find(key);
    if (it != LEGACY_TO_CANONICAL.end()) {
        return it->second;
    }
    
    // Check if it's an INI-format canonical key (git.* or git.repository.*)
    // that should map to the standard YAML format
    static const std::map<std::string, std::string> INI_TO_CANONICAL = {
        {"git.repo_type", "repository.repo_type"},
        {"git.repo_url", "repository.repo_url"},
        {"git.repo_branch", "repository.repo_branch"},
        {"git.repo_path", "repository.repo_path"},
        {"git.repo_mode", "repository.repo_mode"},
        {"git.ssh_key", "repository.ssh_key_path"},
        {"git.sign_commits", "repository.sign_commits"},
        {"git.commit_template", "repository.commit_template"},
        {"git.gpg_key_id", "repository.gpg_key_id"},
        {"git.repository.repo_type", "repository.repo_type"},
        {"git.repository.repo_url", "repository.repo_url"},
        {"git.repository.repo_branch", "repository.repo_branch"},
        {"git.repository.repo_path", "repository.repo_path"},
        {"git.repository.repo_mode", "repository.repo_mode"},
        {"git.repository.ssh_key", "repository.ssh_key_path"},
        {"git.repository.sign_commits", "repository.sign_commits"},
        {"git.repository.commit_template", "repository.commit_template"},
        {"git.repository.gpg_key_id", "repository.gpg_key_id"},
        {"git.schema.include_grants", "schema.include_grants"},
        {"git.schema.include_comments", "schema.include_comments"},
        {"git.schema.include_defaults", "schema.include_defaults"},
        {"git.schema.separate_indexes", "schema.separate_indexes"},
        {"git.schema.file_per_object", "schema.file_per_object"},
        {"git.migrations.table", "migrations.table"},
        {"git.migrations.naming", "migrations.naming"},
        {"git.migrations.generate_down", "migrations.generate_down"},
        {"git.migrations.transaction_per_file", "migrations.transaction_per_file"},
        {"git.migrations.checksum_validation", "migrations.checksum_validation"},
    };
    
    auto ini_it = INI_TO_CANONICAL.find(key);
    if (ini_it != INI_TO_CANONICAL.end()) {
        return ini_it->second;
    }
    
    // Return as-is if no mapping found
    return key;
}

//=============================================================================
// Implementation Details
//=============================================================================

struct GitConfigParser::Impl {
    // Raw YAML content for re-serialization
    std::string raw_content;

    // Simple key-value store for raw access
    std::map<std::string, std::variant<std::string, int, bool,
                                       std::vector<std::string>>> values;
    
    // Track which keys were set (for precedence handling)
    std::set<std::string> canonical_keys_set;
    std::set<std::string> legacy_keys_set;
    
    // Track if we're parsing INI format
    bool is_ini_format = false;
    
    // Store warnings for deprecated key usage
    std::vector<std::string> deprecation_warnings;
    
    /**
     * Store a value with canonical/legacy precedence tracking
     */
    void storeValue(const std::string& raw_key, 
                    const std::variant<std::string, int, bool, std::vector<std::string>>& value,
                    bool is_canonical) {
        std::string canonical_key = normalizeKey(raw_key);
        bool key_is_legacy = isLegacyKey(raw_key);
        
        // Special handling for legacy boolean flags (auto_commit, auto_push, auto_pull)
        // These are legacy but don't map to a single canonical key (they set repo_mode)
        bool is_legacy_boolean_flag = (raw_key == "repository.auto_commit" || 
                                       raw_key == "repository.auto_push" ||
                                       raw_key == "repository.auto_pull");
        
        // Check for precedence conflict
        if (key_is_legacy || is_legacy_boolean_flag) {
            // Track that a legacy key was used (even if it will be ignored due to precedence)
            legacy_keys_set.insert(raw_key);
            
            // Check if canonical version of this key was already set
            if (canonical_keys_set.find(canonical_key) != canonical_keys_set.end()) {
                // Canonical already set, ignore legacy
                deprecation_warnings.push_back(
                    "Deprecated key '" + raw_key + "' ignored - canonical key '" + 
                    canonical_key + "' already set");
                return;
            }
        } else if (is_canonical) {
            canonical_keys_set.insert(canonical_key);
        }
        
        // For legacy boolean flags, store under original key name
        // For other keys, store under canonical key
        if (is_legacy_boolean_flag) {
            values[raw_key] = value;
        } else {
            values[canonical_key] = value;
        }
        
        // Add warning for deprecated key usage
        if (key_is_legacy || is_legacy_boolean_flag) {
            deprecation_warnings.push_back(
                "Deprecated key '" + raw_key + "' used - consider using canonical key '" + 
                canonical_key + "' instead");
        }
    }
    
    /**
     * Get string value with fallback
     */
    std::optional<std::string> getString(const std::string& key) const {
        auto it = values.find(key);
        if (it != values.end()) {
            if (auto* str = std::get_if<std::string>(&it->second)) {
                return *str;
            }
        }
        return std::nullopt;
    }
    
    /**
     * Get bool value with fallback
     */
    std::optional<bool> getBool(const std::string& key) const {
        auto it = values.find(key);
        if (it != values.end()) {
            if (auto* val = std::get_if<bool>(&it->second)) {
                return *val;
            }
            if (auto* str = std::get_if<std::string>(&it->second)) {
                return (*str == "true" || *str == "yes" || *str == "1");
            }
        }
        return std::nullopt;
    }
};

//=============================================================================
// Constructor/Destructor
//=============================================================================

GitConfigParser::GitConfigParser()
    : impl_(std::make_unique<Impl>()) {
}

GitConfigParser::~GitConfigParser() = default;

//=============================================================================
// File Parsing
//=============================================================================

bool GitConfigParser::parseFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        setError("Failed to open config file: " + file_path);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    
    // Detect format based on file extension
    if (file_path.find(".ini") != std::string::npos ||
        file_path.find(".conf") != std::string::npos) {
        impl_->is_ini_format = true;
        return parseINI(buffer.str());
    }
    
    return parseString(buffer.str());
}

bool GitConfigParser::parseString(const std::string& content) {
    impl_->raw_content = content;
    impl_->is_ini_format = false;
    parse_errors_.clear();
    impl_->deprecation_warnings.clear();
    impl_->canonical_keys_set.clear();
    impl_->legacy_keys_set.clear();

    return parseYAML(content);
}

//=============================================================================
// YAML Parsing
//=============================================================================

bool GitConfigParser::parseYAML(const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    std::string current_section;
    std::string current_subsection;
    int indent_level = 0;

    while (std::getline(stream, line)) {
        // Skip comments and empty lines
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        // Trim whitespace
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        size_t last = line.find_last_not_of(" \t\r\n");
        line = line.substr(first, last - first + 1);

        if (line.empty()) continue;

        // Calculate indent
        int new_indent = static_cast<int>(first / 2);

        // Check for section header (key:)
        if (line.back() == ':' && line.find(':') == line.size() - 1) {
            std::string section = line.substr(0, line.size() - 1);
            if (new_indent == 0) {
                current_section = section;
                current_subsection.clear();
            } else if (new_indent == 1) {
                current_subsection = section;
            }
            indent_level = new_indent;
            continue;
        }

        // Parse key: value pairs
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            // Trim key and value
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // Remove quotes
            if (value.size() >= 2 &&
                ((value.front() == '"' && value.back() == '"') ||
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.size() - 2);
            }

            // Build full key path
            std::string full_key;
            if (!current_section.empty()) {
                full_key = current_section;
                if (!current_subsection.empty()) {
                    full_key += "." + current_subsection;
                }
                full_key += "." + key;
            } else {
                full_key = key;
            }

            // Determine if this is a canonical key
            bool is_canonical = !isLegacyKey(full_key);
            
            // Store raw value with precedence tracking
            impl_->storeValue(full_key, value, is_canonical);

            // Parse into specific configs
            parseValue(current_section, current_subsection, key, value, is_canonical);
        }

        // Handle list items (- item)
        if (line[0] == '-') {
            std::string item = line.substr(1);
            item.erase(0, item.find_first_not_of(" \t"));

            std::string list_key;
            if (!current_section.empty()) {
                list_key = current_section;
                if (!current_subsection.empty()) {
                    list_key += "." + current_subsection;
                }
            }

            // Add to list in values map
            auto it = impl_->values.find(list_key);
            if (it != impl_->values.end()) {
                if (auto* vec = std::get_if<std::vector<std::string>>(&it->second)) {
                    vec->push_back(item);
                }
            } else {
                impl_->values[list_key] = std::vector<std::string>{item};
            }
        }
    }

    // Apply parsed values to config structures
    applyParsedValues();

    return parse_errors_.empty();
}

//=============================================================================
// INI Parsing
//=============================================================================

bool GitConfigParser::parseINI(const std::string& content) {
    impl_->raw_content = content;
    impl_->is_ini_format = true;
    parse_errors_.clear();
    impl_->deprecation_warnings.clear();
    impl_->canonical_keys_set.clear();
    impl_->legacy_keys_set.clear();

    std::istringstream stream(content);
    std::string line;
    std::string current_section;

    while (std::getline(stream, line)) {
        // Skip comments and empty lines
        size_t comment_pos = line.find(';');
        if (comment_pos == std::string::npos) {
            comment_pos = line.find('#');
        }
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        // Trim whitespace
        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;
        size_t last = line.find_last_not_of(" \t\r\n");
        line = line.substr(first, last - first + 1);

        if (line.empty()) continue;

        // Parse section header [section]
        if (line.front() == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            continue;
        }

        // Parse key = value pairs
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);

            // Trim key and value
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // Remove quotes
            if (value.size() >= 2 &&
                ((value.front() == '"' && value.back() == '"') ||
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.size() - 2);
            }

            // Build full key path (INI keys are relative to section)
            std::string full_key = current_section + "." + key;
            
            // Determine if this key is legacy:
            // 1. If it's in the LEGACY_TO_CANONICAL map, it's legacy
            // 2. If the key (without section) is a legacy short name, it's legacy
            bool key_is_legacy = isLegacyKey(full_key);
            if (!key_is_legacy) {
                // Check if this is a short/legacy key name (e.g., "url" instead of "repo_url")
                if (current_section == "git" || current_section == "git.repository") {
                    if (key == "type" || key == "url" || key == "branch" || 
                        key == "path" || key == "mode" || key == "ssh_key") {
                        key_is_legacy = true;
                    }
                }
            }
            
            // Canonical keys are those that aren't legacy
            bool is_canonical = !key_is_legacy;

            // Store raw value (this handles precedence - canonical wins over legacy)
            impl_->storeValue(full_key, value, is_canonical);

            // Parse into specific configs (only if canonical or no conflict)
            // The storeValue function already handles precedence, so we parse unconditionally
            // but the applyParsedValues will use the canonical value
            parseINISection(current_section, key, value, is_canonical);
        }
    }

    // Apply parsed values to config structures
    applyParsedValues();

    return parse_errors_.empty();
}

void GitConfigParser::parseINISection(const std::string& section,
                                       const std::string& key,
                                       const std::string& value,
                                       bool is_canonical) {
    // This function is now a no-op - all values are applied via applyParsedValues()
    // The precedence handling is done in storeValue() which stores values in impl_->values
    // applyParsedValues() then reads from impl_->values and applies to config objects
    (void)section;
    (void)key;
    (void)value;
    (void)is_canonical;
}

void GitConfigParser::applyParsedValues() {
    // Helper lambda to get value from either YAML or INI key format
    auto getValue = [this](const std::string& yaml_key, const std::string& ini_key) -> std::optional<std::string> {
        auto val = impl_->getString(yaml_key);
        if (val) return val;
        return impl_->getString(ini_key);
    };
    
    auto getBoolValue = [this](const std::string& yaml_key, const std::string& ini_key) -> std::optional<bool> {
        auto val = impl_->getBool(yaml_key);
        if (val) return val;
        return impl_->getBool(ini_key);
    };
    
    // Apply repository values (with environment variable resolution)
    // Check both YAML format (repository.*) and INI format (git.repository.* or git.*)
    auto repo_url = getValue("repository.repo_url", "git.repository.repo_url");
    if (!repo_url) repo_url = getValue("", "git.repo_url");  // Also check git.url
    if (repo_url) git_config_.repo_url = resolveEnvVars(*repo_url);
    
    auto repo_branch = getValue("repository.repo_branch", "git.repository.repo_branch");
    if (!repo_branch) repo_url = getValue("", "git.repo_branch");
    if (repo_branch) git_config_.repo_branch = resolveEnvVars(*repo_branch);
    
    auto repo_path = getValue("repository.repo_path", "git.repository.repo_path");
    if (!repo_path) repo_path = getValue("", "git.repo_path");
    if (repo_path) git_config_.repo_path = resolveEnvVars(*repo_path);
    
    auto repo_type = getValue("repository.repo_type", "git.repository.repo_type");
    if (!repo_type) repo_type = getValue("", "git.repo_type");
    if (repo_type) git_config_.repo_type = resolveEnvVars(*repo_type);
    
    auto repo_mode = getValue("repository.repo_mode", "git.repository.repo_mode");
    if (!repo_mode) repo_mode = getValue("", "git.repo_mode");
    if (repo_mode) git_config_.repo_mode = resolveEnvVars(*repo_mode);
    
    auto ssh_key = getValue("repository.ssh_key_path", "git.repository.ssh_key_path");
    if (!ssh_key) ssh_key = getValue("", "git.ssh_key");
    if (ssh_key) git_config_.ssh_key_path = resolveEnvVars(*ssh_key);
    
    auto sign_commits = getBoolValue("repository.sign_commits", "git.repository.sign_commits");
    if (!sign_commits) sign_commits = getBoolValue("", "git.sign_commits");
    if (sign_commits) git_config_.sign_commits = *sign_commits;
    
    auto commit_template = getValue("repository.commit_template", "git.repository.commit_template");
    if (!commit_template) commit_template = getValue("", "git.commit_template");
    if (commit_template) git_config_.commit_template = resolveEnvVars(*commit_template);
    
    auto gpg_key_id = getValue("repository.gpg_key_id", "git.repository.gpg_key_id");
    if (!gpg_key_id) gpg_key_id = getValue("", "git.gpg_key_id");
    if (gpg_key_id) git_config_.gpg_key_id = resolveEnvVars(*gpg_key_id);
    
    // Handle legacy boolean flags that map to repo_mode (YAML format only)
    if (!repo_mode) {
        bool auto_commit = impl_->getBool("repository.auto_commit").value_or(false);
        bool auto_push = impl_->getBool("repository.auto_push").value_or(false);
        bool auto_pull = impl_->getBool("repository.auto_pull").value_or(false);
        git_config_.setFromLegacyFlags(auto_commit, auto_push, auto_pull);
    }
    
    // Apply schema options (check both YAML and INI formats)
    auto include_grants = getBoolValue("schema.include_grants", "git.schema.include_grants");
    if (include_grants) schema_options_.include_grants = *include_grants;
    
    auto include_comments = getBoolValue("schema.include_comments", "git.schema.include_comments");
    if (include_comments) schema_options_.include_comments = *include_comments;
    
    auto include_defaults = getBoolValue("schema.include_defaults", "git.schema.include_defaults");
    if (include_defaults) schema_options_.include_defaults = *include_defaults;
    
    auto separate_indexes = getBoolValue("schema.separate_indexes", "git.schema.separate_indexes");
    if (separate_indexes) schema_options_.separate_indexes = *separate_indexes;
    
    auto file_per_object = getBoolValue("schema.file_per_object", "git.schema.file_per_object");
    if (file_per_object) schema_options_.file_per_object = *file_per_object;
    
    // Apply migration config (check both YAML and INI formats)
    auto mig_table = getValue("migrations.table", "git.migrations.table");
    if (mig_table) migration_config_.table_name = *mig_table;
    
    auto mig_naming = getValue("migrations.naming", "git.migrations.naming");
    if (mig_naming) {
        if (*mig_naming == "versioned") migration_config_.naming = MigrationNaming::VERSIONED;
        else if (*mig_naming == "timestamp") migration_config_.naming = MigrationNaming::TIMESTAMP;
        else if (*mig_naming == "sequential") migration_config_.naming = MigrationNaming::SEQUENTIAL;
    }
    
    auto generate_down = getBoolValue("migrations.generate_down", "git.migrations.generate_down");
    if (generate_down) migration_config_.generate_down = *generate_down;
    
    auto transaction_per_file = getBoolValue("migrations.transaction_per_file", "git.migrations.transaction_per_file");
    if (transaction_per_file) migration_config_.transaction_per_file = *transaction_per_file;
    
    auto checksum_validation = getBoolValue("migrations.checksum_validation", "git.migrations.checksum_validation");
    if (checksum_validation) migration_config_.checksum_validation = *checksum_validation;
}

//=============================================================================
// Value Parsing (YAML)
//=============================================================================

void GitConfigParser::parseValue(const std::string& section,
                                  const std::string& subsection,
                                  const std::string& key,
                                  const std::string& value,
                                  bool is_canonical) {
    // Repository section
    if (section == "repository") {
        if (key == "repo_url" || (!is_canonical && key == "url")) {
            git_config_.repo_url = resolveEnvVars(value);
        } else if (key == "repo_branch" || (!is_canonical && key == "branch")) {
            git_config_.repo_branch = value;
        } else if (key == "repo_path" || (!is_canonical && key == "path")) {
            git_config_.repo_path = resolveEnvVars(value);
        } else if (key == "repo_type" || (!is_canonical && key == "type")) {
            git_config_.repo_type = value;
        } else if (key == "repo_mode" || (!is_canonical && key == "mode")) {
            git_config_.repo_mode = value;
        } else if (key == "sign_commits") {
            git_config_.sign_commits = (value == "true" || value == "yes" || value == "1");
        } else if (key == "commit_template") {
            git_config_.commit_template = value;
        } else if (key == "gpg_key_id") {
            git_config_.gpg_key_id = value;
        }
        // Legacy boolean flags - don't set immediately, store for applyParsedValues
        else if (key == "auto_commit" || key == "auto_push" || key == "auto_pull") {
            // These are stored in impl_->values and processed in applyParsedValues
        } else if (key == "ssh_key" || key == "ssh_key_path") {
            git_config_.ssh_key_path = resolveEnvVars(value);
        }
    }

    // Schema section
    else if (section == "schema") {
        if (key == "directory") { /* used for path prefix */ }
        else if (key == "include_grants") schema_options_.include_grants = (value == "true");
        else if (key == "include_comments") schema_options_.include_comments = (value == "true");
        else if (key == "include_defaults") schema_options_.include_defaults = (value == "true");
        else if (key == "separate_indexes") schema_options_.separate_indexes = (value == "true");
        else if (key == "file_per_object") schema_options_.file_per_object = (value == "true");
    }

    // Migrations section
    else if (section == "migrations") {
        if (key == "table") migration_config_.table_name = value;
        else if (key == "naming") {
            if (value == "versioned") migration_config_.naming = MigrationNaming::VERSIONED;
            else if (value == "timestamp") migration_config_.naming = MigrationNaming::TIMESTAMP;
            else if (value == "sequential") migration_config_.naming = MigrationNaming::SEQUENTIAL;
        }
        else if (key == "generate_down") migration_config_.generate_down = (value == "true");
        else if (key == "transaction_per_file") migration_config_.transaction_per_file = (value == "true");
        else if (key == "checksum_validation") migration_config_.checksum_validation = (value == "true");
    }

    // Environments section
    else if (section == "environments" && !subsection.empty()) {
        auto& env = environments_[subsection];
        env.name = subsection;
        if (key == "database") env.database = value;
        else if (key == "host") env.host = value;
        else if (key == "port") env.port = std::stoi(value);
        else if (key == "approval_required") env.approval_required = (value == "true");
        else if (key == "backup_before_apply") env.backup_before_apply = (value == "true");
        else if (key == "auto_apply") { /* dev setting */ }
    }
}

//=============================================================================
// Validation
//=============================================================================

std::vector<std::string> GitConfigParser::validate() const {
    std::vector<std::string> errors;

    // Check required fields - repo_url (or legacy url alias)
    if (git_config_.repo_url.empty()) {
        errors.push_back("repository.repo_url is required");
    }

    // Validate URL format
    if (!git_config_.repo_url.empty() &&
        git_config_.repo_url.find("://") == std::string::npos &&
        git_config_.repo_url.find("git@") != 0) {
        errors.push_back("repository.repo_url must be a valid Git URL");
    }
    
    // Validate repo_mode value
    if (!git_config_.repo_mode.empty()) {
        static const std::set<std::string> valid_modes = {
            "manual", "auto_commit", "auto_push", "full_sync"
        };
        if (valid_modes.find(git_config_.repo_mode) == valid_modes.end()) {
            errors.push_back("repository.repo_mode must be one of: manual, auto_commit, auto_push, full_sync");
        }
    }
    
    // Validate repo_type
    if (!git_config_.repo_type.empty() && git_config_.repo_type != "git") {
        errors.push_back("repository.repo_type must be 'git' (other types not yet supported)");
    }

    return errors;
}

//=============================================================================
// Configuration Access
//=============================================================================

GitConfig GitConfigParser::getGitConfig() const {
    return git_config_;
}

SchemaOptions GitConfigParser::getSchemaOptions() const {
    return schema_options_;
}

MigrationConfig GitConfigParser::getMigrationConfig() const {
    return migration_config_;
}

std::optional<EnvironmentConfig> GitConfigParser::getEnvironment(
    const std::string& env_name) const {
    auto it = environments_.find(env_name);
    if (it != environments_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::string> GitConfigParser::listEnvironments() const {
    std::vector<std::string> envs;
    for (const auto& [name, _] : environments_) {
        envs.push_back(name);
    }
    return envs;
}

//=============================================================================
// Hook Configuration
//=============================================================================

std::vector<std::string> GitConfigParser::getHooks(HookType type) const {
    auto it = hooks_.find(type);
    if (it != hooks_.end()) {
        return it->second;
    }
    return {};
}

//=============================================================================
// Environment Variable Substitution
//=============================================================================

std::string GitConfigParser::resolveEnvVars(const std::string& value) const {
    std::string result = value;
    std::regex env_regex(R"(\$\{([^}]+)\})");
    std::smatch match;

    std::string::const_iterator search_start = result.cbegin();
    std::string resolved;
    size_t last_pos = 0;

    while (std::regex_search(search_start, result.cend(), match, env_regex)) {
        // Add text before match
        resolved += result.substr(last_pos, match.position());

        std::string var_name = match[1];

        // Check custom env vars first
        auto it = env_vars_.find(var_name);
        if (it != env_vars_.end()) {
            resolved += it->second;
        } else {
            // Fall back to system environment
            const char* env_value = std::getenv(var_name.c_str());
            if (env_value) {
                resolved += env_value;
            } else {
                // Keep original if not found
                resolved += match[0];
            }
        }

        last_pos = match.position() + match.length();
        search_start = match.suffix().first;
    }

    // Add remaining text
    resolved += result.substr(last_pos);

    return resolved;
}

void GitConfigParser::setEnvVar(const std::string& name, const std::string& value) {
    env_vars_[name] = value;
}

//=============================================================================
// Diagnostics
//=============================================================================

std::vector<std::string> GitConfigParser::getDeprecationWarnings() const {
    return impl_->deprecation_warnings;
}

bool GitConfigParser::hasCanonicalKeys() const {
    return !impl_->canonical_keys_set.empty();
}

bool GitConfigParser::hasLegacyKeys() const {
    return !impl_->legacy_keys_set.empty();
}

//=============================================================================
// Serialization
//=============================================================================

std::string GitConfigParser::toYAML() const {
    std::stringstream yaml;

    yaml << "# ScratchBird Git Integration Configuration\n";
    yaml << "# Canonical keys are preferred. See documentation for details.\n";
    yaml << "version: 1\n\n";

    // Repository - emit canonical keys only
    yaml << "repository:\n";
    yaml << "  repo_type: " << git_config_.repo_type << "\n";
    yaml << "  repo_url: \"" << git_config_.repo_url << "\"\n";
    yaml << "  repo_branch: " << git_config_.repo_branch << "\n";
    if (!git_config_.repo_path.empty()) {
        yaml << "  repo_path: \"" << git_config_.repo_path << "\"\n";
    }
    yaml << "  repo_mode: " << git_config_.repo_mode << "\n";
    if (git_config_.sign_commits) {
        yaml << "  sign_commits: true\n";
    }
    if (!git_config_.commit_template.empty()) {
        yaml << "  commit_template: \"" << git_config_.commit_template << "\"\n";
    }
    if (!git_config_.gpg_key_id.empty()) {
        yaml << "  gpg_key_id: \"" << git_config_.gpg_key_id << "\"\n";
    }
    if (!git_config_.ssh_key_path.empty()) {
        yaml << "  ssh_key_path: \"" << git_config_.ssh_key_path << "\"\n";
    }
    yaml << "\n";

    // Schema
    yaml << "schema:\n";
    yaml << "  include_grants: " << (schema_options_.include_grants ? "true" : "false") << "\n";
    yaml << "  include_comments: " << (schema_options_.include_comments ? "true" : "false") << "\n";
    yaml << "  include_defaults: " << (schema_options_.include_defaults ? "true" : "false") << "\n";
    yaml << "  separate_indexes: " << (schema_options_.separate_indexes ? "true" : "false") << "\n";
    yaml << "  file_per_object: " << (schema_options_.file_per_object ? "true" : "false") << "\n";

    if (!schema_options_.include_schemas.empty()) {
        yaml << "  schemas:\n";
        for (const auto& s : schema_options_.include_schemas) {
            yaml << "    - " << s << "\n";
        }
    }

    if (!schema_options_.exclude_schemas.empty()) {
        yaml << "  exclude_schemas:\n";
        for (const auto& s : schema_options_.exclude_schemas) {
            yaml << "    - " << s << "\n";
        }
    }
    yaml << "\n";

    // Migrations
    yaml << "migrations:\n";
    yaml << "  table: " << migration_config_.table_name << "\n";
    yaml << "  naming: ";
    switch (migration_config_.naming) {
        case MigrationNaming::VERSIONED: yaml << "versioned\n"; break;
        case MigrationNaming::TIMESTAMP: yaml << "timestamp\n"; break;
        case MigrationNaming::SEQUENTIAL: yaml << "sequential\n"; break;
    }
    yaml << "  generate_down: " << (migration_config_.generate_down ? "true" : "false") << "\n";
    yaml << "  transaction_per_file: " << (migration_config_.transaction_per_file ? "true" : "false") << "\n";
    yaml << "  checksum_validation: " << (migration_config_.checksum_validation ? "true" : "false") << "\n";
    yaml << "\n";

    // Environments
    if (!environments_.empty()) {
        yaml << "environments:\n";
        for (const auto& [name, env] : environments_) {
            yaml << "  " << name << ":\n";
            yaml << "    database: " << env.database << "\n";
            if (!env.host.empty()) {
                yaml << "    host: " << env.host << "\n";
            }
            if (env.port != 3092) {
                yaml << "    port: " << env.port << "\n";
            }
            if (env.approval_required) {
                yaml << "    approval_required: true\n";
            }
            if (env.backup_before_apply) {
                yaml << "    backup_before_apply: true\n";
            }
        }
        yaml << "\n";
    }

    // Hooks
    bool has_hooks = false;
    for (const auto& [type, cmds] : hooks_) {
        if (!cmds.empty()) has_hooks = true;
    }

    if (has_hooks) {
        yaml << "hooks:\n";
        auto write_hooks = [&yaml](const char* name,
                                    const std::vector<std::string>& cmds) {
            yaml << "  " << name << ":\n";
            for (const auto& cmd : cmds) {
                yaml << "    - " << cmd << "\n";
            }
        };

        for (const auto& [type, cmds] : hooks_) {
            if (cmds.empty()) continue;
            switch (type) {
                case HookType::PRE_EXPORT: write_hooks("pre_export", cmds); break;
                case HookType::POST_EXPORT: write_hooks("post_export", cmds); break;
                case HookType::PRE_IMPORT: write_hooks("pre_import", cmds); break;
                case HookType::POST_IMPORT: write_hooks("post_import", cmds); break;
                case HookType::PRE_MIGRATE: write_hooks("pre_migrate", cmds); break;
                case HookType::POST_MIGRATE: write_hooks("post_migrate", cmds); break;
                case HookType::PRE_ROLLBACK: write_hooks("pre_rollback", cmds); break;
                case HookType::POST_ROLLBACK: write_hooks("post_rollback", cmds); break;
            }
        }
    }

    return yaml.str();
}

bool GitConfigParser::writeFile(const std::string& file_path) const {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        return false;
    }

    file << toYAML();
    return true;
}

//=============================================================================
// Raw Value Access
//=============================================================================

std::optional<std::string> GitConfigParser::getString(const std::string& path) const {
    // Map to canonical key
    std::string canonical_path = getCanonicalKey(path);
    
    auto it = impl_->values.find(canonical_path);
    if (it != impl_->values.end()) {
        if (auto* str = std::get_if<std::string>(&it->second)) {
            return *str;
        }
    }
    return std::nullopt;
}

std::optional<int> GitConfigParser::getInt(const std::string& path) const {
    std::string canonical_path = getCanonicalKey(path);
    
    auto it = impl_->values.find(canonical_path);
    if (it != impl_->values.end()) {
        if (auto* val = std::get_if<int>(&it->second)) {
            return *val;
        }
        if (auto* str = std::get_if<std::string>(&it->second)) {
            try {
                return std::stoi(*str);
            } catch (...) {}
        }
    }
    return std::nullopt;
}

std::optional<bool> GitConfigParser::getBool(const std::string& path) const {
    std::string canonical_path = getCanonicalKey(path);
    
    auto it = impl_->values.find(canonical_path);
    if (it != impl_->values.end()) {
        if (auto* val = std::get_if<bool>(&it->second)) {
            return *val;
        }
        if (auto* str = std::get_if<std::string>(&it->second)) {
            return (*str == "true" || *str == "yes" || *str == "1");
        }
    }
    return std::nullopt;
}

std::vector<std::string> GitConfigParser::getStringList(const std::string& path) const {
    std::string canonical_path = getCanonicalKey(path);
    
    auto it = impl_->values.find(canonical_path);
    if (it != impl_->values.end()) {
        if (auto* vec = std::get_if<std::vector<std::string>>(&it->second)) {
            return *vec;
        }
    }
    return {};
}

//=============================================================================
// Modification
//=============================================================================

void GitConfigParser::setGitConfig(const GitConfig& config) {
    git_config_ = config;
}

void GitConfigParser::setSchemaOptions(const SchemaOptions& options) {
    schema_options_ = options;
}

void GitConfigParser::setMigrationConfig(const MigrationConfig& config) {
    migration_config_ = config;
}

void GitConfigParser::addEnvironment(const EnvironmentConfig& env) {
    environments_[env.name] = env;
}

void GitConfigParser::addHook(HookType type, const std::string& command) {
    hooks_[type].push_back(command);
}

void GitConfigParser::setValue(const std::string& path,
                                const std::variant<std::string, int, bool,
                                                   std::vector<std::string>>& value) {
    impl_->values[path] = value;
}

//=============================================================================
// Error Handling
//=============================================================================

std::string GitConfigParser::getLastError() const {
    return last_error_;
}

std::vector<std::string> GitConfigParser::getParseErrors() const {
    return parse_errors_;
}

void GitConfigParser::setError(const std::string& error) {
    last_error_ = error;
}

void GitConfigParser::addParseError(const std::string& error) {
    parse_errors_.push_back(error);
}

} // namespace git
} // namespace scratchbird

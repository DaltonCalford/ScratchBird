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

namespace scratchbird {
namespace git {

//=============================================================================
// Implementation Details
//=============================================================================

struct GitConfigParser::Impl {
    // Raw YAML content for re-serialization
    std::string raw_content;

    // Simple key-value store for raw access
    std::map<std::string, std::variant<std::string, int, bool,
                                       std::vector<std::string>>> values;
};

//=============================================================================
// Constructor/Destructor
//=============================================================================

GitConfigParser::GitConfigParser()
    : impl_(std::make_unique<Impl>()) {
}

GitConfigParser::~GitConfigParser() = default;

//=============================================================================
// Parsing
//=============================================================================

bool GitConfigParser::parseFile(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        setError("Failed to open config file: " + file_path);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parseString(buffer.str());
}

bool GitConfigParser::parseString(const std::string& content) {
    impl_->raw_content = content;
    parse_errors_.clear();

    // Simple YAML-like parser
    // In production, would use a proper YAML library like yaml-cpp

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

            // Store raw value
            impl_->values[full_key] = value;

            // Parse into specific configs
            parseValue(current_section, current_subsection, key, value);
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

    return parse_errors_.empty();
}

void GitConfigParser::parseValue(const std::string& section,
                                  const std::string& subsection,
                                  const std::string& key,
                                  const std::string& value) {
    // Repository section
    if (section == "repository") {
        if (key == "url") git_config_.url = resolveEnvVars(value);
        else if (key == "branch") git_config_.branch = value;
        else if (key == "auto_commit") git_config_.auto_commit = (value == "true");
        else if (key == "auto_push") git_config_.auto_push = (value == "true");
        else if (key == "auto_pull") git_config_.auto_pull = (value == "true");
        else if (key == "ssh_key") git_config_.ssh_key_path = resolveEnvVars(value);
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

std::vector<std::string> GitConfigParser::validate() const {
    std::vector<std::string> errors;

    // Check required fields
    if (git_config_.url.empty()) {
        errors.push_back("repository.url is required");
    }

    // Validate URL format
    if (!git_config_.url.empty() &&
        git_config_.url.find("://") == std::string::npos &&
        git_config_.url.find("git@") != 0) {
        errors.push_back("repository.url must be a valid Git URL");
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
// Serialization
//=============================================================================

std::string GitConfigParser::toYAML() const {
    std::stringstream yaml;

    yaml << "# ScratchBird Git Integration Configuration\n";
    yaml << "version: 1\n\n";

    // Repository
    yaml << "repository:\n";
    yaml << "  url: \"" << git_config_.url << "\"\n";
    yaml << "  branch: " << git_config_.branch << "\n";
    yaml << "  auto_commit: " << (git_config_.auto_commit ? "true" : "false") << "\n";
    yaml << "  auto_push: " << (git_config_.auto_push ? "true" : "false") << "\n";
    if (!git_config_.ssh_key_path.empty()) {
        yaml << "  ssh_key: \"" << git_config_.ssh_key_path << "\"\n";
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
    auto it = impl_->values.find(path);
    if (it != impl_->values.end()) {
        if (auto* str = std::get_if<std::string>(&it->second)) {
            return *str;
        }
    }
    return std::nullopt;
}

std::optional<int> GitConfigParser::getInt(const std::string& path) const {
    auto it = impl_->values.find(path);
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
    auto it = impl_->values.find(path);
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
    auto it = impl_->values.find(path);
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

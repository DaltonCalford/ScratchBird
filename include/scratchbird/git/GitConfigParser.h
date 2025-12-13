/*
 * ScratchBird Database Engine
 * Git Configuration Parser - YAML Configuration Parsing
 * Copyright (c) 2025 ScratchBird Project
 */
#pragma once

#include "GitTypes.h"
#include <memory>
#include <variant>

namespace scratchbird {
namespace git {

/**
 * GitConfigParser parses .scratchbird.yml configuration files.
 *
 * Supports:
 * - Repository settings
 * - Schema export/import options
 * - Migration configuration
 * - Environment-specific overrides
 * - Hook definitions
 */
class GitConfigParser {
public:
    GitConfigParser();
    ~GitConfigParser();

    // Non-copyable
    GitConfigParser(const GitConfigParser&) = delete;
    GitConfigParser& operator=(const GitConfigParser&) = delete;

    //=========================================================================
    // Parsing
    //=========================================================================

    /**
     * Parse configuration from file
     * @param file_path Path to .scratchbird.yml
     * @return true on success
     */
    bool parseFile(const std::string& file_path);

    /**
     * Parse configuration from string
     * @param content YAML content
     * @return true on success
     */
    bool parseString(const std::string& content);

    /**
     * Validate current configuration
     * @return List of validation errors (empty if valid)
     */
    std::vector<std::string> validate() const;

    //=========================================================================
    // Configuration Access
    //=========================================================================

    /**
     * Get Git repository configuration
     */
    GitConfig getGitConfig() const;

    /**
     * Get schema export/import options
     */
    SchemaOptions getSchemaOptions() const;

    /**
     * Get migration configuration
     */
    MigrationConfig getMigrationConfig() const;

    /**
     * Get environment configuration
     * @param env_name Environment name
     * @return Environment config or nullopt
     */
    std::optional<EnvironmentConfig> getEnvironment(
        const std::string& env_name) const;

    /**
     * List available environments
     */
    std::vector<std::string> listEnvironments() const;

    //=========================================================================
    // Hook Configuration
    //=========================================================================

    /**
     * Hook types
     */
    enum class HookType {
        PRE_EXPORT,
        POST_EXPORT,
        PRE_IMPORT,
        POST_IMPORT,
        PRE_MIGRATE,
        POST_MIGRATE,
        PRE_ROLLBACK,
        POST_ROLLBACK
    };

    /**
     * Get hooks for a specific type
     * @param type Hook type
     * @return List of hook commands
     */
    std::vector<std::string> getHooks(HookType type) const;

    //=========================================================================
    // Environment Variable Substitution
    //=========================================================================

    /**
     * Resolve environment variable references in a string
     * @param value String with ${VAR} references
     * @return Resolved string
     */
    std::string resolveEnvVars(const std::string& value) const;

    /**
     * Set environment variable for resolution
     * @param name Variable name
     * @param value Variable value
     */
    void setEnvVar(const std::string& name, const std::string& value);

    //=========================================================================
    // Serialization
    //=========================================================================

    /**
     * Generate YAML from current configuration
     * @return YAML string
     */
    std::string toYAML() const;

    /**
     * Write configuration to file
     * @param file_path Output file path
     * @return true on success
     */
    bool writeFile(const std::string& file_path) const;

    //=========================================================================
    // Raw Value Access
    //=========================================================================

    /**
     * Get raw string value
     * @param path Dot-separated path (e.g., "repository.url")
     * @return Value or nullopt
     */
    std::optional<std::string> getString(const std::string& path) const;

    /**
     * Get raw integer value
     * @param path Dot-separated path
     * @return Value or nullopt
     */
    std::optional<int> getInt(const std::string& path) const;

    /**
     * Get raw boolean value
     * @param path Dot-separated path
     * @return Value or nullopt
     */
    std::optional<bool> getBool(const std::string& path) const;

    /**
     * Get string list value
     * @param path Dot-separated path
     * @return List of strings
     */
    std::vector<std::string> getStringList(const std::string& path) const;

    //=========================================================================
    // Modification
    //=========================================================================

    /**
     * Set Git configuration
     */
    void setGitConfig(const GitConfig& config);

    /**
     * Set schema options
     */
    void setSchemaOptions(const SchemaOptions& options);

    /**
     * Set migration configuration
     */
    void setMigrationConfig(const MigrationConfig& config);

    /**
     * Add environment configuration
     */
    void addEnvironment(const EnvironmentConfig& env);

    /**
     * Add hook
     */
    void addHook(HookType type, const std::string& command);

    /**
     * Set raw value
     * @param path Dot-separated path
     * @param value Value to set
     */
    void setValue(const std::string& path,
                  const std::variant<std::string, int, bool,
                                     std::vector<std::string>>& value);

    //=========================================================================
    // Error Handling
    //=========================================================================

    /**
     * Get last error message
     */
    std::string getLastError() const;

    /**
     * Get parse errors
     */
    std::vector<std::string> getParseErrors() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    GitConfig git_config_;
    SchemaOptions schema_options_;
    MigrationConfig migration_config_;
    std::map<std::string, EnvironmentConfig> environments_;
    std::map<HookType, std::vector<std::string>> hooks_;
    std::map<std::string, std::string> env_vars_;
    std::string last_error_;
    std::vector<std::string> parse_errors_;

    void setError(const std::string& error);
    void addParseError(const std::string& error);
    bool parseGitSection(const std::string& yaml);
    bool parseSchemaSection(const std::string& yaml);
    bool parseMigrationSection(const std::string& yaml);
    bool parseEnvironmentsSection(const std::string& yaml);
    bool parseHooksSection(const std::string& yaml);
};

/**
 * Default configuration template
 */
inline std::string getDefaultConfigTemplate() {
    return R"(# ScratchBird Git Integration Configuration
# Version: 1

version: 1

# Repository settings
repository:
  type: git
  url: ""                    # Git repository URL
  branch: main               # Default branch
  auto_commit: false         # Auto-commit on export
  auto_push: false           # Auto-push after commit

# Schema export settings
schema:
  directory: schema          # Schema files directory
  include_grants: true       # Include GRANT statements
  include_comments: true     # Include COMMENT ON statements
  include_defaults: true     # Include DEFAULT values
  separate_indexes: true     # Export indexes as separate files
  file_per_object: true      # One file per object
  schemas:                   # Schemas to include (empty = all)
    - public
  exclude_schemas:           # Schemas to exclude
    - pg_catalog
    - information_schema
  exclude_tables: []         # Tables to exclude (glob patterns)

# Migration settings
migrations:
  directory: migrations      # Migration files directory
  table: SYS$MIGRATIONS      # Migration tracking table
  naming: versioned          # versioned, timestamp, or sequential
  generate_down: true        # Generate rollback scripts
  transaction_per_file: true # Wrap each file in transaction
  checksum_validation: true  # Validate file checksums

# Environment configurations
environments:
  development:
    database: dev_db
    auto_apply: true
  staging:
    database: staging_db
    approval_required: false
  production:
    database: prod_db
    approval_required: true
    backup_before_apply: true

# Hooks
hooks:
  pre_export: []
  post_export: []
  pre_migrate: []
  post_migrate: []
)";
}

} // namespace git
} // namespace scratchbird

/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

/**
 * ScratchBird Configuration Parser
 *
 * Alpha 3 Phase 3.3: Service Mode & systemd Integration
 *
 * INI-style configuration file parser supporting:
 * - Sections [section]
 * - Key-value pairs: key = value
 * - Comments: # or ;
 * - Environment variable expansion: ${VAR} or ${VAR:-default}
 * - Include directives: @include /path/to/file.conf
 * - Size values: 128MB, 1GB, etc.
 * - Time values: 30s, 5m, 1h, etc.
 */

#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <variant>
#include <optional>
#include <functional>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird {
namespace server {

// ============================================================================
// Configuration Value Types
// ============================================================================

/**
 * Parsed configuration value
 */
class ConfigValue {
public:
    ConfigValue() = default;
    explicit ConfigValue(const std::string& str) : raw_value_(str) {}
    explicit ConfigValue(int64_t val) : raw_value_(std::to_string(val)) {}
    explicit ConfigValue(double val) : raw_value_(std::to_string(val)) {}
    explicit ConfigValue(bool val) : raw_value_(val ? "true" : "false") {}

    // Raw string value
    const std::string& raw() const { return raw_value_; }

    // Type conversions
    std::string asString(const std::string& default_val = "") const;
    int64_t asInt(int64_t default_val = 0) const;
    double asDouble(double default_val = 0.0) const;
    bool asBool(bool default_val = false) const;

    // Size parsing (128MB -> bytes)
    uint64_t asSize(uint64_t default_val = 0) const;

    // Duration parsing (30s -> milliseconds)
    uint64_t asDuration(uint64_t default_val = 0) const;

    // List parsing (comma-separated)
    std::vector<std::string> asList(char delimiter = ',') const;

    // Check if value is set
    bool isSet() const { return !raw_value_.empty(); }

    // Comparison
    bool operator==(const ConfigValue& other) const { return raw_value_ == other.raw_value_; }
    bool operator!=(const ConfigValue& other) const { return raw_value_ != other.raw_value_; }

private:
    std::string raw_value_;
};

// ============================================================================
// Configuration Section
// ============================================================================

/**
 * Configuration section containing key-value pairs
 */
class ConfigSection {
public:
    ConfigSection() = default;
    explicit ConfigSection(const std::string& name) : name_(name) {}

    const std::string& name() const { return name_; }

    // Get value
    ConfigValue get(const std::string& key) const;
    ConfigValue get(const std::string& key, const ConfigValue& default_val) const;

    // Typed getters with defaults
    std::string getString(const std::string& key, const std::string& default_val = "") const;
    int64_t getInt(const std::string& key, int64_t default_val = 0) const;
    double getDouble(const std::string& key, double default_val = 0.0) const;
    bool getBool(const std::string& key, bool default_val = false) const;
    uint64_t getSize(const std::string& key, uint64_t default_val = 0) const;
    uint64_t getDuration(const std::string& key, uint64_t default_val = 0) const;
    std::vector<std::string> getList(const std::string& key, char delimiter = ',') const;

    // Set value
    void set(const std::string& key, const ConfigValue& value);
    void set(const std::string& key, const std::string& value);
    void set(const std::string& key, int64_t value);
    void set(const std::string& key, bool value);

    // Check if key exists
    bool has(const std::string& key) const;

    // Remove key
    void remove(const std::string& key);

    // Get all keys
    std::vector<std::string> keys() const;

    // Get all key-value pairs
    const std::map<std::string, ConfigValue>& values() const { return values_; }

private:
    std::string name_;
    std::map<std::string, ConfigValue> values_;
};

// ============================================================================
// Configuration File Parser
// ============================================================================

/**
 * Parse error information
 */
struct ConfigParseError {
    std::string file;           // File where error occurred
    int line;                   // Line number (1-based)
    std::string message;        // Error description
    std::string context;        // Line content or context

    std::string toString() const;
};

/**
 * Configuration file parser options
 */
struct ConfigParserOptions {
    bool allow_includes = true;         // Allow @include directives
    bool expand_env = true;             // Expand ${VAR} environment variables
    bool strict_mode = false;           // Fail on unknown sections/keys
    int max_include_depth = 10;         // Maximum include nesting depth
    std::vector<std::string> include_paths;  // Directories to search for includes
};

/**
 * ConfigParser - INI-style configuration file parser
 */
class ConfigParser {
public:
    ConfigParser();
    explicit ConfigParser(const ConfigParserOptions& options);

    // ========================================================================
    // Parsing
    // ========================================================================

    /**
     * Parse configuration file
     *
     * @param path Path to configuration file
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status parseFile(const std::string& path, core::ErrorContext* ctx = nullptr);

    /**
     * Parse configuration from string
     *
     * @param content Configuration content
     * @param source_name Source name for error messages
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status parseString(const std::string& content,
                             const std::string& source_name = "<string>",
                             core::ErrorContext* ctx = nullptr);

    /**
     * Get parse errors
     */
    const std::vector<ConfigParseError>& errors() const { return errors_; }

    /**
     * Clear all parsed configuration
     */
    void clear();

    // ========================================================================
    // Configuration Access
    // ========================================================================

    /**
     * Get a section
     *
     * @param name Section name (case-insensitive)
     * @return Pointer to section, or nullptr if not found
     */
    const ConfigSection* section(const std::string& name) const;
    ConfigSection* section(const std::string& name);

    /**
     * Get or create a section
     */
    ConfigSection& getOrCreateSection(const std::string& name);

    /**
     * Check if section exists
     */
    bool hasSection(const std::string& name) const;

    /**
     * Get all section names
     */
    std::vector<std::string> sectionNames() const;

    /**
     * Get value from section.key notation
     *
     * @param path "section.key" format
     * @return Value or empty ConfigValue if not found
     */
    ConfigValue get(const std::string& path) const;

    /**
     * Convenience typed getters using section.key notation
     */
    std::string getString(const std::string& path, const std::string& default_val = "") const;
    int64_t getInt(const std::string& path, int64_t default_val = 0) const;
    bool getBool(const std::string& path, bool default_val = false) const;
    uint64_t getSize(const std::string& path, uint64_t default_val = 0) const;
    uint64_t getDuration(const std::string& path, uint64_t default_val = 0) const;

    // ========================================================================
    // Modification
    // ========================================================================

    /**
     * Set value using section.key notation
     */
    void set(const std::string& path, const ConfigValue& value);
    void set(const std::string& path, const std::string& value);

    /**
     * Merge another configuration (values from other override this)
     */
    void merge(const ConfigParser& other);

    // ========================================================================
    // Serialization
    // ========================================================================

    /**
     * Serialize configuration to INI format
     */
    std::string serialize() const;

    /**
     * Write configuration to file
     */
    core::Status writeFile(const std::string& path, core::ErrorContext* ctx = nullptr) const;

    // ========================================================================
    // Validation
    // ========================================================================

    /**
     * Validation callback type
     * Returns empty string on success, error message on failure
     */
    using Validator = std::function<std::string(const ConfigValue&)>;

    /**
     * Register a validator for a key
     */
    void addValidator(const std::string& path, Validator validator);

    /**
     * Validate all registered validators
     *
     * @return true if all validations pass
     */
    bool validate();

    /**
     * Get validation errors
     */
    const std::vector<std::string>& validationErrors() const { return validation_errors_; }

private:
    // Internal parsing state
    struct ParseState {
        std::string current_file;
        int current_line = 0;
        std::string current_section;
        int include_depth = 0;
    };

    // Parse a single line
    core::Status parseLine(const std::string& line, ParseState& state, core::ErrorContext* ctx);

    // Parse include directive
    core::Status parseInclude(const std::string& path, ParseState& state, core::ErrorContext* ctx);

    // Expand environment variables in a string
    std::string expandEnvVars(const std::string& value) const;

    // Resolve include path
    std::string resolveIncludePath(const std::string& path, const std::string& current_file) const;

    // Add parse error
    void addError(const ParseState& state, const std::string& message);

    // Normalize section name (lowercase)
    static std::string normalizeName(const std::string& name);

    // Split path into section.key
    static bool splitPath(const std::string& path, std::string& section, std::string& key);

    ConfigParserOptions options_;
    std::map<std::string, ConfigSection> sections_;
    std::vector<ConfigParseError> errors_;
    std::map<std::string, Validator> validators_;
    std::vector<std::string> validation_errors_;
};

// ============================================================================
// Size and Duration Parsing Utilities
// ============================================================================

/**
 * Parse size string to bytes
 *
 * Supports: B, KB, K, MB, M, GB, G, TB, T
 * Examples: "128MB", "1GB", "4096"
 *
 * @param str Size string
 * @param bytes Output bytes
 * @return true if parsed successfully
 */
bool parseSize(const std::string& str, uint64_t& bytes);

/**
 * Parse duration string to milliseconds
 *
 * Supports: ms, s, m, h, d
 * Examples: "30s", "5m", "1h", "1000ms", "500"
 *
 * @param str Duration string
 * @param milliseconds Output milliseconds
 * @return true if parsed successfully
 */
bool parseDuration(const std::string& str, uint64_t& milliseconds);

/**
 * Format size in bytes to human-readable string
 */
std::string formatSize(uint64_t bytes);

/**
 * Format duration in milliseconds to human-readable string
 */
std::string formatDuration(uint64_t milliseconds);

// ============================================================================
// Configuration Search Utilities
// ============================================================================

/**
 * Search for configuration file in standard locations
 *
 * Search order:
 * 1. Explicit path (if provided)
 * 2. SCRATCHBIRD_CONFIG environment variable
 * 3. ./sb_server.conf
 * 4. ~/.config/scratchbird/sb_server.conf
 * 5. /etc/scratchbird/sb_server.conf
 *
 * @param explicit_path Explicit path to check first (optional)
 * @return Path to config file if found, empty string otherwise
 */
std::string findConfigFile(const std::string& explicit_path = "");

/**
 * Get default configuration directory
 *
 * @return /etc/scratchbird (system) or ~/.config/scratchbird (user)
 */
std::string getConfigDirectory(bool system = true);

}  // namespace server
}  // namespace scratchbird

/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * ScratchBird Configuration Parser Implementation
 *
 * Alpha 3 Phase 3.3: Service Mode & systemd Integration
 */

#include "scratchbird/server/config_parser.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include "scratchbird/core/posix_compat.h"
#include <pwd.h>
#include <sys/types.h>
#endif

namespace scratchbird {
namespace server {

// ============================================================================
// ConfigValue Implementation
// ============================================================================

std::string ConfigValue::asString(const std::string& default_val) const {
    return raw_value_.empty() ? default_val : raw_value_;
}

int64_t ConfigValue::asInt(int64_t default_val) const {
    if (raw_value_.empty()) return default_val;
    try {
        return std::stoll(raw_value_);
    } catch (...) {
        return default_val;
    }
}

double ConfigValue::asDouble(double default_val) const {
    if (raw_value_.empty()) return default_val;
    try {
        return std::stod(raw_value_);
    } catch (...) {
        return default_val;
    }
}

bool ConfigValue::asBool(bool default_val) const {
    if (raw_value_.empty()) return default_val;

    std::string lower = raw_value_;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "true" || lower == "yes" || lower == "on" || lower == "1") {
        return true;
    }
    if (lower == "false" || lower == "no" || lower == "off" || lower == "0") {
        return false;
    }
    return default_val;
}

uint64_t ConfigValue::asSize(uint64_t default_val) const {
    if (raw_value_.empty()) return default_val;
    uint64_t result;
    if (parseSize(raw_value_, result)) {
        return result;
    }
    return default_val;
}

uint64_t ConfigValue::asDuration(uint64_t default_val) const {
    if (raw_value_.empty()) return default_val;
    uint64_t result;
    if (parseDuration(raw_value_, result)) {
        return result;
    }
    return default_val;
}

std::vector<std::string> ConfigValue::asList(char delimiter) const {
    std::vector<std::string> result;
    if (raw_value_.empty()) return result;

    std::istringstream ss(raw_value_);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        // Trim whitespace
        size_t start = item.find_first_not_of(" \t");
        size_t end = item.find_last_not_of(" \t");
        if (start != std::string::npos && end != std::string::npos) {
            result.push_back(item.substr(start, end - start + 1));
        } else if (start != std::string::npos) {
            result.push_back(item.substr(start));
        }
    }
    return result;
}

// ============================================================================
// ConfigSection Implementation
// ============================================================================

ConfigValue ConfigSection::get(const std::string& key) const {
    auto it = values_.find(key);
    return it != values_.end() ? it->second : ConfigValue();
}

ConfigValue ConfigSection::get(const std::string& key, const ConfigValue& default_val) const {
    auto it = values_.find(key);
    return it != values_.end() ? it->second : default_val;
}

std::string ConfigSection::getString(const std::string& key, const std::string& default_val) const {
    return get(key).asString(default_val);
}

int64_t ConfigSection::getInt(const std::string& key, int64_t default_val) const {
    return get(key).asInt(default_val);
}

double ConfigSection::getDouble(const std::string& key, double default_val) const {
    return get(key).asDouble(default_val);
}

bool ConfigSection::getBool(const std::string& key, bool default_val) const {
    return get(key).asBool(default_val);
}

uint64_t ConfigSection::getSize(const std::string& key, uint64_t default_val) const {
    return get(key).asSize(default_val);
}

uint64_t ConfigSection::getDuration(const std::string& key, uint64_t default_val) const {
    return get(key).asDuration(default_val);
}

std::vector<std::string> ConfigSection::getList(const std::string& key, char delimiter) const {
    return get(key).asList(delimiter);
}

void ConfigSection::set(const std::string& key, const ConfigValue& value) {
    values_[key] = value;
}

void ConfigSection::set(const std::string& key, const std::string& value) {
    values_[key] = ConfigValue(value);
}

void ConfigSection::set(const std::string& key, int64_t value) {
    values_[key] = ConfigValue(value);
}

void ConfigSection::set(const std::string& key, bool value) {
    values_[key] = ConfigValue(value);
}

bool ConfigSection::has(const std::string& key) const {
    return values_.find(key) != values_.end();
}

void ConfigSection::remove(const std::string& key) {
    values_.erase(key);
}

std::vector<std::string> ConfigSection::keys() const {
    std::vector<std::string> result;
    result.reserve(values_.size());
    for (const auto& kv : values_) {
        result.push_back(kv.first);
    }
    return result;
}

// ============================================================================
// ConfigParseError Implementation
// ============================================================================

std::string ConfigParseError::toString() const {
    std::ostringstream ss;
    ss << file << ":" << line << ": " << message;
    if (!context.empty()) {
        ss << " (near: " << context << ")";
    }
    return ss.str();
}

// ============================================================================
// ConfigParser Implementation
// ============================================================================

ConfigParser::ConfigParser()
    : options_() {}

ConfigParser::ConfigParser(const ConfigParserOptions& options)
    : options_(options) {}

core::Status ConfigParser::parseFile(const std::string& path, core::ErrorContext* ctx) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (ctx) {
            std::string msg = "Failed to open configuration file: " + path;
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, msg.c_str());
        }
        return core::Status::IO_ERROR;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    ParseState state;
    state.current_file = path;
    state.current_line = 0;
    state.include_depth = 0;

    return parseString(buffer.str(), path, ctx);
}

core::Status ConfigParser::parseString(const std::string& content,
                                       const std::string& source_name,
                                       core::ErrorContext* ctx) {
    ParseState state;
    state.current_file = source_name;
    state.current_line = 0;

    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        state.current_line++;
        core::Status status = parseLine(line, state, ctx);
        if (status != core::Status::OK && options_.strict_mode) {
            return status;
        }
    }

    return errors_.empty() ? core::Status::OK : core::Status::INVALID_ARGUMENT;
}

core::Status ConfigParser::parseLine(const std::string& line, ParseState& state,
                                     core::ErrorContext* ctx) {
    // Trim whitespace
    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return core::Status::OK;  // Empty line
    }

    size_t end = line.find_last_not_of(" \t\r\n");
    std::string trimmed = line.substr(start, end - start + 1);

    // Skip comments
    if (trimmed[0] == '#' || trimmed[0] == ';') {
        return core::Status::OK;
    }

    // Handle include directive
    if (trimmed.substr(0, 8) == "@include") {
        if (!options_.allow_includes) {
            addError(state, "Include directives are disabled");
            return core::Status::INVALID_ARGUMENT;
        }

        std::string include_path = trimmed.substr(8);
        // Trim whitespace and quotes
        size_t path_start = include_path.find_first_not_of(" \t\"'");
        size_t path_end = include_path.find_last_not_of(" \t\"'");
        if (path_start != std::string::npos && path_end != std::string::npos) {
            include_path = include_path.substr(path_start, path_end - path_start + 1);
        }

        return parseInclude(include_path, state, ctx);
    }

    // Handle section header [section]
    if (trimmed[0] == '[') {
        size_t close = trimmed.find(']');
        if (close == std::string::npos) {
            addError(state, "Missing closing bracket in section header");
            return core::Status::INVALID_ARGUMENT;
        }

        state.current_section = normalizeName(trimmed.substr(1, close - 1));
        getOrCreateSection(state.current_section);
        return core::Status::OK;
    }

    // Handle key = value
    size_t eq = trimmed.find('=');
    if (eq == std::string::npos) {
        addError(state, "Invalid configuration line (missing '=')");
        return core::Status::INVALID_ARGUMENT;
    }

    std::string key = trimmed.substr(0, eq);
    std::string value = trimmed.substr(eq + 1);

    // Trim key
    size_t key_end = key.find_last_not_of(" \t");
    if (key_end != std::string::npos) {
        key = key.substr(0, key_end + 1);
    }

    // Trim value and handle quotes
    size_t val_start = value.find_first_not_of(" \t");
    if (val_start != std::string::npos) {
        value = value.substr(val_start);
    }
    size_t val_end = value.find_last_not_of(" \t");
    if (val_end != std::string::npos) {
        value = value.substr(0, val_end + 1);
    }

    // Remove surrounding quotes if present
    if (value.size() >= 2) {
        if ((value[0] == '"' && value.back() == '"') ||
            (value[0] == '\'' && value.back() == '\'')) {
            value = value.substr(1, value.size() - 2);
        }
    }

    // Strip inline comments (not inside quotes)
    size_t comment_pos = value.find('#');
    if (comment_pos != std::string::npos) {
        // Check if # is inside quotes - simplified check
        bool in_quotes = false;
        for (size_t i = 0; i < comment_pos; i++) {
            if (value[i] == '"' || value[i] == '\'') {
                in_quotes = !in_quotes;
            }
        }
        if (!in_quotes) {
            value = value.substr(0, comment_pos);
            // Re-trim
            val_end = value.find_last_not_of(" \t");
            if (val_end != std::string::npos) {
                value = value.substr(0, val_end + 1);
            }
        }
    }

    // Expand environment variables
    if (options_.expand_env) {
        value = expandEnvVars(value);
    }

    // Use default section if none specified
    if (state.current_section.empty()) {
        state.current_section = "default";
    }

    // Set the value
    getOrCreateSection(state.current_section).set(key, value);

    return core::Status::OK;
}

core::Status ConfigParser::parseInclude(const std::string& path, ParseState& state,
                                        core::ErrorContext* ctx) {
    if (state.include_depth >= options_.max_include_depth) {
        addError(state, "Maximum include depth exceeded");
        return core::Status::INVALID_ARGUMENT;
    }

    std::string resolved_path = resolveIncludePath(path, state.current_file);

    // Handle glob patterns
    if (resolved_path.find('*') != std::string::npos) {
        try {
            std::filesystem::path pattern_path(resolved_path);
            std::filesystem::path parent = pattern_path.parent_path();

            if (std::filesystem::exists(parent)) {
                for (const auto& entry : std::filesystem::directory_iterator(parent)) {
                    std::string filename = entry.path().filename().string();
                    // Simple glob matching - just check extension
                    std::string pattern_name = pattern_path.filename().string();
                    size_t star = pattern_name.find('*');
                    if (star != std::string::npos) {
                        std::string prefix = pattern_name.substr(0, star);
                        std::string suffix = pattern_name.substr(star + 1);
                        if (filename.substr(0, prefix.size()) == prefix &&
                            filename.size() >= suffix.size() &&
                            filename.substr(filename.size() - suffix.size()) == suffix) {
                            // Match - include this file
                            ParseState include_state = state;
                            include_state.current_file = entry.path().string();
                            include_state.current_line = 0;
                            include_state.include_depth++;

                            std::ifstream file(entry.path());
                            if (file.is_open()) {
                                std::stringstream buffer;
                                buffer << file.rdbuf();
                                file.close();

                                std::istringstream stream(buffer.str());
                                std::string line;
                                while (std::getline(stream, line)) {
                                    include_state.current_line++;
                                    parseLine(line, include_state, ctx);
                                }
                            }
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            addError(state, std::string("Include glob error: ") + e.what());
        }
        return core::Status::OK;
    }

    // Single file include
    if (!std::filesystem::exists(resolved_path)) {
        addError(state, "Include file not found: " + resolved_path);
        return core::Status::IO_ERROR;
    }

    ParseState include_state = state;
    include_state.current_file = resolved_path;
    include_state.current_line = 0;
    include_state.include_depth++;

    std::ifstream file(resolved_path);
    if (!file.is_open()) {
        addError(state, "Failed to open include file: " + resolved_path);
        return core::Status::IO_ERROR;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    std::istringstream stream(buffer.str());
    std::string line;
    while (std::getline(stream, line)) {
        include_state.current_line++;
        parseLine(line, include_state, ctx);
    }

    return core::Status::OK;
}

std::string ConfigParser::expandEnvVars(const std::string& value) const {
    std::string result = value;
    size_t pos = 0;

    while ((pos = result.find("${", pos)) != std::string::npos) {
        size_t end = result.find('}', pos);
        if (end == std::string::npos) break;

        std::string var_expr = result.substr(pos + 2, end - pos - 2);
        std::string var_name;
        std::string default_val;

        // Check for default value syntax: ${VAR:-default}
        size_t default_pos = var_expr.find(":-");
        if (default_pos != std::string::npos) {
            var_name = var_expr.substr(0, default_pos);
            default_val = var_expr.substr(default_pos + 2);
        } else {
            var_name = var_expr;
        }

        const char* env_val = std::getenv(var_name.c_str());
        std::string replacement = env_val ? env_val : default_val;

        result.replace(pos, end - pos + 1, replacement);
        pos += replacement.size();
    }

    return result;
}

std::string ConfigParser::resolveIncludePath(const std::string& path,
                                             const std::string& current_file) const {
    // Absolute path
    if (!path.empty() && (path[0] == '/' || (path.size() > 1 && path[1] == ':'))) {
        return path;
    }

    // Relative to current file
    std::filesystem::path current(current_file);
    std::filesystem::path resolved = current.parent_path() / path;
    if (std::filesystem::exists(resolved)) {
        return resolved.string();
    }

    // Search in include paths
    for (const auto& include_path : options_.include_paths) {
        std::filesystem::path candidate = std::filesystem::path(include_path) / path;
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }

    // Return as-is (will fail with file not found)
    return path;
}

void ConfigParser::addError(const ParseState& state, const std::string& message) {
    ConfigParseError error;
    error.file = state.current_file;
    error.line = state.current_line;
    error.message = message;
    errors_.push_back(error);
}

std::string ConfigParser::normalizeName(const std::string& name) {
    std::string result = name;
    // Trim whitespace
    size_t start = result.find_first_not_of(" \t");
    size_t end = result.find_last_not_of(" \t");
    if (start != std::string::npos && end != std::string::npos) {
        result = result.substr(start, end - start + 1);
    }
    // Convert to lowercase
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

bool ConfigParser::splitPath(const std::string& path, std::string& section, std::string& key) {
    size_t dot = path.find('.');
    if (dot == std::string::npos) {
        section = "default";
        key = path;
    } else {
        section = normalizeName(path.substr(0, dot));
        key = path.substr(dot + 1);
    }
    return true;
}

void ConfigParser::clear() {
    sections_.clear();
    errors_.clear();
    validation_errors_.clear();
}

const ConfigSection* ConfigParser::section(const std::string& name) const {
    std::string normalized = normalizeName(name);
    auto it = sections_.find(normalized);
    return it != sections_.end() ? &it->second : nullptr;
}

ConfigSection* ConfigParser::section(const std::string& name) {
    std::string normalized = normalizeName(name);
    auto it = sections_.find(normalized);
    return it != sections_.end() ? &it->second : nullptr;
}

ConfigSection& ConfigParser::getOrCreateSection(const std::string& name) {
    std::string normalized = normalizeName(name);
    auto it = sections_.find(normalized);
    if (it == sections_.end()) {
        sections_.emplace(normalized, ConfigSection(normalized));
        return sections_.at(normalized);
    }
    return it->second;
}

bool ConfigParser::hasSection(const std::string& name) const {
    return sections_.find(normalizeName(name)) != sections_.end();
}

std::vector<std::string> ConfigParser::sectionNames() const {
    std::vector<std::string> result;
    result.reserve(sections_.size());
    for (const auto& kv : sections_) {
        result.push_back(kv.first);
    }
    return result;
}

ConfigValue ConfigParser::get(const std::string& path) const {
    std::string section_name, key;
    splitPath(path, section_name, key);

    const ConfigSection* sec = section(section_name);
    return sec ? sec->get(key) : ConfigValue();
}

std::string ConfigParser::getString(const std::string& path, const std::string& default_val) const {
    return get(path).asString(default_val);
}

int64_t ConfigParser::getInt(const std::string& path, int64_t default_val) const {
    return get(path).asInt(default_val);
}

bool ConfigParser::getBool(const std::string& path, bool default_val) const {
    return get(path).asBool(default_val);
}

uint64_t ConfigParser::getSize(const std::string& path, uint64_t default_val) const {
    return get(path).asSize(default_val);
}

uint64_t ConfigParser::getDuration(const std::string& path, uint64_t default_val) const {
    return get(path).asDuration(default_val);
}

void ConfigParser::set(const std::string& path, const ConfigValue& value) {
    std::string section_name, key;
    splitPath(path, section_name, key);
    getOrCreateSection(section_name).set(key, value);
}

void ConfigParser::set(const std::string& path, const std::string& value) {
    set(path, ConfigValue(value));
}

void ConfigParser::merge(const ConfigParser& other) {
    for (const auto& kv : other.sections_) {
        ConfigSection& section = getOrCreateSection(kv.first);
        for (const auto& val : kv.second.values()) {
            section.set(val.first, val.second);
        }
    }
}

std::string ConfigParser::serialize() const {
    std::ostringstream ss;

    for (const auto& section_kv : sections_) {
        ss << "[" << section_kv.first << "]\n";
        for (const auto& val_kv : section_kv.second.values()) {
            ss << val_kv.first << " = " << val_kv.second.raw() << "\n";
        }
        ss << "\n";
    }

    return ss.str();
}

core::Status ConfigParser::writeFile(const std::string& path, core::ErrorContext* ctx) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        if (ctx) {
            std::string msg = "Failed to create configuration file: " + path;
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, msg.c_str());
        }
        return core::Status::IO_ERROR;
    }

    file << serialize();
    file.close();

    return core::Status::OK;
}

void ConfigParser::addValidator(const std::string& path, Validator validator) {
    validators_[path] = std::move(validator);
}

bool ConfigParser::validate() {
    validation_errors_.clear();

    for (const auto& kv : validators_) {
        ConfigValue value = get(kv.first);
        std::string error = kv.second(value);
        if (!error.empty()) {
            validation_errors_.push_back(kv.first + ": " + error);
        }
    }

    return validation_errors_.empty();
}

// ============================================================================
// Size and Duration Parsing
// ============================================================================

bool parseSize(const std::string& str, uint64_t& bytes) {
    if (str.empty()) return false;

    std::string s = str;
    // Trim whitespace
    size_t start = s.find_first_not_of(" \t");
    size_t end = s.find_last_not_of(" \t");
    if (start == std::string::npos) return false;
    s = s.substr(start, end - start + 1);

    // Find where digits end
    size_t unit_start = 0;
    while (unit_start < s.size() && (std::isdigit(s[unit_start]) || s[unit_start] == '.')) {
        unit_start++;
    }

    if (unit_start == 0) return false;

    double value;
    try {
        value = std::stod(s.substr(0, unit_start));
    } catch (...) {
        return false;
    }

    std::string unit = s.substr(unit_start);
    // Trim and uppercase
    start = unit.find_first_not_of(" \t");
    if (start != std::string::npos) {
        unit = unit.substr(start);
    }
    std::transform(unit.begin(), unit.end(), unit.begin(), ::toupper);

    uint64_t multiplier = 1;
    if (unit.empty() || unit == "B") {
        multiplier = 1;
    } else if (unit == "K" || unit == "KB" || unit == "KIB") {
        multiplier = 1024ULL;
    } else if (unit == "M" || unit == "MB" || unit == "MIB") {
        multiplier = 1024ULL * 1024;
    } else if (unit == "G" || unit == "GB" || unit == "GIB") {
        multiplier = 1024ULL * 1024 * 1024;
    } else if (unit == "T" || unit == "TB" || unit == "TIB") {
        multiplier = 1024ULL * 1024 * 1024 * 1024;
    } else {
        return false;
    }

    bytes = static_cast<uint64_t>(value * multiplier);
    return true;
}

bool parseDuration(const std::string& str, uint64_t& milliseconds) {
    if (str.empty()) return false;

    std::string s = str;
    // Trim whitespace
    size_t start = s.find_first_not_of(" \t");
    size_t end = s.find_last_not_of(" \t");
    if (start == std::string::npos) return false;
    s = s.substr(start, end - start + 1);

    // Find where digits end
    size_t unit_start = 0;
    while (unit_start < s.size() && (std::isdigit(s[unit_start]) || s[unit_start] == '.')) {
        unit_start++;
    }

    if (unit_start == 0) return false;

    double value;
    try {
        value = std::stod(s.substr(0, unit_start));
    } catch (...) {
        return false;
    }

    std::string unit = s.substr(unit_start);
    // Trim and lowercase
    start = unit.find_first_not_of(" \t");
    if (start != std::string::npos) {
        unit = unit.substr(start);
    }
    std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);

    uint64_t multiplier = 1;
    if (unit.empty() || unit == "ms") {
        multiplier = 1;
    } else if (unit == "s" || unit == "sec" || unit == "second" || unit == "seconds") {
        multiplier = 1000;
    } else if (unit == "m" || unit == "min" || unit == "minute" || unit == "minutes") {
        multiplier = 1000 * 60;
    } else if (unit == "h" || unit == "hr" || unit == "hour" || unit == "hours") {
        multiplier = 1000 * 60 * 60;
    } else if (unit == "d" || unit == "day" || unit == "days") {
        multiplier = 1000 * 60 * 60 * 24;
    } else {
        // Try parsing as pure number (assumed milliseconds)
        try {
            milliseconds = std::stoull(str);
            return true;
        } catch (...) {
            return false;
        }
    }

    milliseconds = static_cast<uint64_t>(value * multiplier);
    return true;
}

std::string formatSize(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024 && unit_idx < 4) {
        size /= 1024;
        unit_idx++;
    }

    std::ostringstream ss;
    if (unit_idx == 0) {
        ss << bytes << units[unit_idx];
    } else {
        ss << std::fixed;
        ss.precision(size >= 100 ? 0 : (size >= 10 ? 1 : 2));
        ss << size << units[unit_idx];
    }
    return ss.str();
}

std::string formatDuration(uint64_t milliseconds) {
    if (milliseconds < 1000) {
        return std::to_string(milliseconds) + "ms";
    }

    uint64_t seconds = milliseconds / 1000;
    if (seconds < 60) {
        return std::to_string(seconds) + "s";
    }

    uint64_t minutes = seconds / 60;
    if (minutes < 60) {
        return std::to_string(minutes) + "m";
    }

    uint64_t hours = minutes / 60;
    if (hours < 24) {
        return std::to_string(hours) + "h";
    }

    uint64_t days = hours / 24;
    return std::to_string(days) + "d";
}

// ============================================================================
// Configuration File Search
// ============================================================================

std::string findConfigFile(const std::string& explicit_path) {
    // 1. Explicit path
    if (!explicit_path.empty()) {
        if (std::filesystem::exists(explicit_path)) {
            return explicit_path;
        }
    }

    // 2. Environment variable
    const char* env_config = std::getenv("SCRATCHBIRD_CONFIG");
    if (env_config && std::filesystem::exists(env_config)) {
        return env_config;
    }

    // 3. Current directory
    if (std::filesystem::exists("./sb_server.conf")) {
        return "./sb_server.conf";
    }

    // 4. User config directory
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::string user_config = std::string(path) + "\\scratchbird\\sb_server.conf";
        if (std::filesystem::exists(user_config)) {
            return user_config;
        }
    }
#else
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (home) {
        std::string user_config = std::string(home) + "/.config/scratchbird/sb_server.conf";
        if (std::filesystem::exists(user_config)) {
            return user_config;
        }
    }
#endif

    // 5. System config
    if (std::filesystem::exists("/etc/scratchbird/sb_server.conf")) {
        return "/etc/scratchbird/sb_server.conf";
    }

    return "";
}

std::string getConfigDirectory(bool system) {
    if (system) {
#ifdef _WIN32
        return "C:\\ProgramData\\scratchbird";
#else
        return "/etc/scratchbird";
#endif
    }

    // User directory
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        return std::string(path) + "\\scratchbird";
    }
    return "";
#else
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (home) {
        return std::string(home) + "/.config/scratchbird";
    }
    return "";
#endif
}

}  // namespace server
}  // namespace scratchbird

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
 * Configuration Management Implementation
 * Implements INI file parsing, environment variable support, and command-line overrides
 * Phase 1: Foundation Infrastructure
 */
// Section 37 invariant: configuration metadata here is a runtime and bootstrap
// surface only. It must not be misread as durable schema-catalog ownership or
// as proof of core metadata visibility semantics.

#include "scratchbird/core/config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>
#include <unordered_set>

namespace
{
    using scratchbird::core::config::CatalogHotApplyClass;
    using scratchbird::core::config::CatalogKeyDefinition;
    using scratchbird::core::config::CatalogScope;
    using scratchbird::core::config::CatalogValueType;

    std::string trimAsciiCopy(std::string value)
    {
        const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
        value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), is_space));
        value.erase(std::find_if_not(value.rbegin(), value.rend(), is_space).base(), value.end());
        return value;
    }

    std::string toLowerAsciiCopy(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    std::vector<std::string> splitCommaList(std::string_view raw)
    {
        std::vector<std::string> values;
        std::string current;
        for (char ch : raw)
        {
            if (ch == ',')
            {
                current = trimAsciiCopy(current);
                if (!current.empty())
                {
                    values.push_back(current);
                }
                current.clear();
                continue;
            }
            current.push_back(ch);
        }

        current = trimAsciiCopy(current);
        if (!current.empty())
        {
            values.push_back(current);
        }
        return values;
    }

    auto catalogDefinitionsStorage() -> const std::vector<CatalogKeyDefinition> &
    {
        static const std::vector<CatalogKeyDefinition> definitions = {
            {1, "engine.database_root", CatalogValueType::STRING, CatalogScope::INSTANCE, "",
             nullptr, nullptr, nullptr, true, true, true, false,
             CatalogHotApplyClass::NONE, false, "Bootstrap database root."},
            {2, "engine.database_uuid", CatalogValueType::STRING, CatalogScope::DATABASE, "",
             nullptr, nullptr, nullptr, false, false, true, false,
             CatalogHotApplyClass::NONE, false, "Database UUID mirror."},
            {3, "engine.mode", CatalogValueType::STRING, CatalogScope::INSTANCE, "embedded",
             nullptr, nullptr, "embedded,ipc_server", true, true, true, false,
             CatalogHotApplyClass::NONE, false, "Engine runtime mode."},
            {4, "storage.default_filespace_path", CatalogValueType::STRING,
             CatalogScope::INSTANCE, "", nullptr, nullptr, nullptr, true, true, true, false,
             CatalogHotApplyClass::NONE, false, "Default filespace path."},
            {5, "storage.default_page_size", CatalogValueType::UINT64, CatalogScope::INSTANCE,
             "16384", "4096", "65536", nullptr, true, true, true, false,
             CatalogHotApplyClass::NONE, false, "Default database page size."},
            {6, "storage.filespace.autoextend", CatalogValueType::BOOL, CatalogScope::INSTANCE,
             "true", nullptr, nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::NONE, false, "Filespace autoextend enablement."},
            {7, "storage.filespace.extend_chunk_pages", CatalogValueType::UINT64,
             CatalogScope::INSTANCE, "128", "1", nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::NONE, false, "Filespace extend chunk size."},
            {8, "ipc.enabled", CatalogValueType::BOOL, CatalogScope::INSTANCE, "true",
             nullptr, nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "IPC listener enabled."},
            {9, "ipc.bind_address", CatalogValueType::STRING, CatalogScope::INSTANCE,
             "127.0.0.1", nullptr, nullptr, nullptr, true, true, false, false,
             CatalogHotApplyClass::NONE, false, "IPC bind address."},
            {10, "ipc.port", CatalogValueType::UINT64, CatalogScope::INSTANCE, "3092",
             "1", "65535", nullptr, true, true, false, false,
             CatalogHotApplyClass::NONE, false, "IPC port."},
            {11, "listener.max_connections_total", CatalogValueType::UINT64,
             CatalogScope::INSTANCE, "100", "1", nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Total listener connections."},
            {12, "listener.accept_backlog", CatalogValueType::UINT64, CatalogScope::INSTANCE,
             "128", "1", nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Listener backlog."},
            {13, "listener.assignment_timeout_ms", CatalogValueType::UINT64,
             CatalogScope::INSTANCE, "5000", "0", nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Assignment timeout."},
            {14, "listener.handshake_timeout_ms", CatalogValueType::UINT64,
             CatalogScope::INSTANCE, "5000", "0", nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Handshake timeout."},
            {15, "listener.idle_connection_timeout_ms", CatalogValueType::UINT64,
             CatalogScope::INSTANCE, "3600000", "0", nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Idle connection timeout."},
            {16, "listener.reject_when_no_open_database", CatalogValueType::BOOL,
             CatalogScope::INSTANCE, "true", nullptr, nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Reject when no open database."},
            {17, "audit.track_session_context", CatalogValueType::BOOL, CatalogScope::INSTANCE,
             "true", nullptr, nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Track session audit context."},
            {18, "audit.track_connection_context", CatalogValueType::BOOL,
             CatalogScope::INSTANCE, "true", nullptr, nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Track connection audit context."},
            {19, "logging.level", CatalogValueType::STRING, CatalogScope::INSTANCE, "info",
             nullptr, nullptr, "debug,info,notice,warning,error", false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Logging level."},
            {20, "logging.path", CatalogValueType::STRING, CatalogScope::INSTANCE, "",
             nullptr, nullptr, nullptr, true, true, true, false,
             CatalogHotApplyClass::NONE, false, "Log file path."},
            {21, "diagnostics.metrics.enabled", CatalogValueType::BOOL,
             CatalogScope::INSTANCE, "true", nullptr, nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Metrics emission."},
            {22, "security.auth_methods", CatalogValueType::LIST_STRING,
             CatalogScope::INSTANCE, "scram", nullptr, nullptr, "scram,cert,trust", false, true,
             false, true, CatalogHotApplyClass::POST_COMMIT_LOCAL, false,
             "Enabled authentication methods."},
            {23, "security.mfa.enabled", CatalogValueType::BOOL, CatalogScope::INSTANCE,
             "false", nullptr, nullptr, nullptr, false, true, false, true,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "MFA enablement."},
            {24, "security.encryption.enabled", CatalogValueType::BOOL,
             CatalogScope::INSTANCE, "false", nullptr, nullptr, nullptr, false, true, false, true,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Encryption enablement."},
            {25, "i18n.resource_bundle_path", CatalogValueType::STRING, CatalogScope::INSTANCE,
             "", nullptr, nullptr, nullptr, true, true, true, false,
             CatalogHotApplyClass::NONE, false, "I18N resource path."},
            {26, "i18n.bootstrap_required", CatalogValueType::BOOL, CatalogScope::INSTANCE,
             "false", nullptr, nullptr, nullptr, true, true, true, false,
             CatalogHotApplyClass::NONE, false, "I18N bootstrap requirement."},
            {27, "i18n.bundle_update_mode", CatalogValueType::STRING, CatalogScope::INSTANCE,
             "manual", nullptr, nullptr, "manual,replace,merge", false, true, true, false,
             CatalogHotApplyClass::NONE, false, "I18N bundle update mode."},
            {28, "timezone.resource_bundle_path", CatalogValueType::STRING,
             CatalogScope::INSTANCE, "", nullptr, nullptr, nullptr, true, true, true, false,
             CatalogHotApplyClass::NONE, false, "Timezone resource path."},
            {29, "timezone.default_name", CatalogValueType::STRING, CatalogScope::INSTANCE,
             "UTC", nullptr, nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Default timezone."},
            {30, "types.default_charset", CatalogValueType::STRING, CatalogScope::INSTANCE,
             "UTF8", nullptr, nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Default charset."},
            {31, "types.default_collation", CatalogValueType::STRING, CatalogScope::INSTANCE,
             "UNICODE", nullptr, nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Default collation."},
            {32, "scheduler.enabled", CatalogValueType::BOOL, CatalogScope::INSTANCE,
             "true", nullptr, nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Scheduler enablement."},
            {33, "transactions.dormant_restart_reattach_policy", CatalogValueType::STRING,
             CatalogScope::INSTANCE, "allow_replacement", nullptr, nullptr,
             "allow_replacement,deny_after_restart", false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Dormant restart reattach policy."},
            {34, "transactions.dormant_cleanup_policy", CatalogValueType::STRING,
             CatalogScope::INSTANCE, "rollback_expired", nullptr, nullptr,
             "keep,expire_only,rollback_expired,rollback_expired_and_purge", false, true, false,
             false, CatalogHotApplyClass::POST_COMMIT_LOCAL, false,
             "Dormant cleanup policy."},
            {35, "transactions.dormant_lease_seconds", CatalogValueType::UINT64,
             CatalogScope::INSTANCE, "3600", "0", nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false, "Dormant lease seconds."},
            {36, "transactions.dormant_terminal_retention_seconds", CatalogValueType::UINT64,
             CatalogScope::INSTANCE, "300", "0", nullptr, nullptr, false, true, false, false,
             CatalogHotApplyClass::POST_COMMIT_LOCAL, false,
             "Dormant terminal retention seconds."},
        };
        return definitions;
    }

    auto dedicatedTopologyKeysStorage() -> const std::unordered_set<std::string> &
    {
        static const std::unordered_set<std::string> keys = {
            "listener.native.enabled",
            "listener.native.port",
            "listener.postgresql.enabled",
            "listener.postgresql.port",
            "listener.mysql.enabled",
            "listener.mysql.port",
            "listener.firebird.enabled",
            "listener.firebird.port",
            "listener.cassandra.enabled",
            "listener.cassandra.port",
            "listener.mongodb.enabled",
            "listener.mongodb.port",
            "listener.neo4j.enabled",
            "listener.neo4j.port",
            "listener.redis.enabled",
            "listener.redis.port",
            "listener.milvus.enabled",
            "listener.milvus.port",
            "listener.bind_address",
            "listener.mgmt_ipc.enabled",
            "listener.mgmt_ipc.bind_address",
            "listener.mgmt_ipc.port",
            "parser.pool.min",
            "parser.pool.max",
            "parser.pool.queue_max",
            "parser.pool.queue_timeout_ms",
            "parser.pool.idle_timeout_ms",
            "parser.pool.spawn_backoff_ms",
            "parser.pool.health_interval_ms",
            "parser.pool.missed_heartbeat_threshold",
            "parser.pool.warm_replenish_timeout_ms",
        };
        return keys;
    }
}

namespace scratchbird::core
{

    // Singleton instance
    Config &Config::getInstance()
    {
        static Config instance;
        return instance;
    }

    Status Config::initialize(const std::string &config_file, ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        config_file_path_ = config_file;

        // Try to load config file (optional - not an error if missing)
        Status s = parseFile(config_file, ctx);
        if (s != Status::OK && s != Status::NOT_FOUND)
        {
            // Real error (not just file not found)
            return s;
        }

        loaded_ = true;
        return Status::OK;
    }

    Status Config::loadFile(const std::string &config_file, ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        config_file_path_ = config_file;
        config_data_.clear(); // Clear existing config

        Status s = parseFile(config_file, ctx);
        if (s == Status::OK)
        {
            loaded_ = true;
        }
        return s;
    }

    void Config::addCommandLineArg(const std::string &section, const std::string &key,
                                   const std::string &value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cmdline_data_[section][key] = value;
    }

    std::string Config::getString(const std::string &section, const std::string &key,
                                  const std::string &default_value) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto val = resolveValueLocked(section, key, true);
        return val.has_value() ? val->value : default_value;
    }

    int64_t Config::getInt(const std::string &section, const std::string &key,
                           int64_t default_value) const
    {
        std::string val = getString(section, key, "");
        if (val.empty())
        {
            return default_value;
        }

        try
        {
            return std::stoll(val);
        }
        catch (...)
        {
            return default_value;
        }
    }

    uint64_t Config::getUInt(const std::string &section, const std::string &key,
                             uint64_t default_value) const
    {
        std::string val = getString(section, key, "");
        if (val.empty())
        {
            return default_value;
        }

        try
        {
            return std::stoull(val);
        }
        catch (...)
        {
            return default_value;
        }
    }

    bool Config::getBool(const std::string &section, const std::string &key,
                         bool default_value) const
    {
        std::string val = getString(section, key, "");
        if (val.empty())
        {
            return default_value;
        }

        // Convert to lowercase for case-insensitive comparison
        std::string lower_val = toLower(val);

        // Check for true values
        if (lower_val == "true" || lower_val == "yes" || lower_val == "on" || lower_val == "1")
        {
            return true;
        }

        // Check for false values
        if (lower_val == "false" || lower_val == "no" || lower_val == "off" || lower_val == "0")
        {
            return false;
        }

        // Invalid boolean, return default
        return default_value;
    }

    double Config::getDouble(const std::string &section, const std::string &key,
                             double default_value) const
    {
        std::string val = getString(section, key, "");
        if (val.empty())
        {
            return default_value;
        }

        try
        {
            return std::stod(val);
        }
        catch (...)
        {
            return default_value;
        }
    }

    bool Config::hasKey(const std::string &section, const std::string &key) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return resolveValueLocked(section, key, true).has_value();
    }

    void Config::set(const std::string &section, const std::string &key, const std::string &value)
    {
        setDurableOverride(section, key, value);
    }

    void Config::setDurableOverride(const std::string &section,
                                    const std::string &key,
                                    const std::string &value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        durable_data_[section][key] = value;
    }

    void Config::unsetDurableOverride(const std::string &section, const std::string &key)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = durable_data_.find(section);
        if (it == durable_data_.end())
        {
            return;
        }
        it->second.erase(key);
        if (it->second.empty())
        {
            durable_data_.erase(it);
        }
    }

    void Config::clearDurableOverrides()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        durable_data_.clear();
    }

    bool Config::hasDurableOverride(const std::string &section, const std::string &key) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = durable_data_.find(section);
        if (it == durable_data_.end())
        {
            return false;
        }
        return it->second.find(key) != it->second.end();
    }

    std::optional<Config::ResolvedValue> Config::getResolvedValue(const std::string &section,
                                                                  const std::string &key) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return resolveValueLocked(section, key, true);
    }

    std::optional<Config::ResolvedValue> Config::getBootstrapResolvedValue(
        const std::string &section,
        const std::string &key) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return resolveValueLocked(section, key, false);
    }

    std::vector<std::string> Config::getKeys(const std::string &section) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::set<std::string> keys;

        auto collect = [&](const auto &source) {
            auto it = source.find(section);
            if (it == source.end())
            {
                return;
            }
            for (const auto &kv : it->second)
            {
                keys.insert(kv.first);
            }
        };

        collect(config_data_);
        collect(durable_data_);
        collect(cmdline_data_);

        return std::vector<std::string>(keys.begin(), keys.end());
    }

    std::vector<std::string> Config::getSections() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::set<std::string> sections;

        auto collect = [&](const auto &source) {
            for (const auto &section : source)
            {
                sections.insert(section.first);
            }
        };

        collect(config_data_);
        collect(durable_data_);
        collect(cmdline_data_);

        return std::vector<std::string>(sections.begin(), sections.end());
    }

    Status Config::reload(ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (config_file_path_.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No config file path set");
            return Status::INVALID_ARGUMENT;
        }

        // Clear existing data and reload
        config_data_.clear();
        return parseFile(config_file_path_, ctx);
    }

    void Config::clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_data_.clear();
        durable_data_.clear();
        cmdline_data_.clear();
        config_file_path_.clear();
        loaded_ = false;
    }

    // Private methods

    std::optional<Config::ResolvedValue> Config::resolveValueLocked(const std::string &section,
                                                                    const std::string &key,
                                                                    bool include_durable) const
    {
        if (include_durable)
        {
            auto durable_section = durable_data_.find(section);
            if (durable_section != durable_data_.end())
            {
                auto durable_key = durable_section->second.find(key);
                if (durable_key != durable_section->second.end())
                {
                    return ResolvedValue{durable_key->second, ValueSource::DURABLE_OVERRIDE};
                }
            }
        }

        // Priority 1: Command-line arguments
        auto cmd_section = cmdline_data_.find(section);
        if (cmd_section != cmdline_data_.end())
        {
            auto cmd_key = cmd_section->second.find(key);
            if (cmd_key != cmd_section->second.end())
            {
                return ResolvedValue{cmd_key->second, ValueSource::COMMAND_LINE};
            }
        }

        // Priority 2: Environment variables
        auto env_val = getEnvVar(section, key);
        if (env_val.has_value())
        {
            return ResolvedValue{env_val.value(), ValueSource::ENVIRONMENT};
        }

        // Priority 3: Config file
        auto section_it = config_data_.find(section);
        if (section_it != config_data_.end())
        {
            auto key_it = section_it->second.find(key);
            if (key_it != section_it->second.end())
            {
                return ResolvedValue{key_it->second, ValueSource::CONFIG_FILE};
            }
        }

        // Not found
        return std::nullopt;
    }

    Status Config::parseFile(const std::string &filepath, ErrorContext *ctx)
    {
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Config file not found: " + filepath).c_str());
            return Status::NOT_FOUND;
        }

        std::string line;
        std::string current_section;
        int line_number = 0;

        while (std::getline(file, line))
        {
            line_number++;
            parseLine(line, current_section);
        }

        file.close();
        return Status::OK;
    }

    void Config::parseLine(const std::string &line, std::string &current_section)
    {
        // Trim whitespace
        std::string trimmed = trim(line);

        // Skip empty lines and comments
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';')
        {
            return;
        }

        // Check for section header [section]
        if (trimmed[0] == '[')
        {
            size_t end = trimmed.find(']');
            if (end != std::string::npos)
            {
                current_section = trim(trimmed.substr(1, end - 1));
            }
            return;
        }

        // Parse key=value
        size_t equals = trimmed.find('=');
        if (equals != std::string::npos)
        {
            std::string key = trim(trimmed.substr(0, equals));
            std::string value = trim(trimmed.substr(equals + 1));

            // Remove quotes from value if present
            if (value.length() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                                        (value.front() == '\'' && value.back() == '\'')))
            {
                value = value.substr(1, value.length() - 2);
            }

            if (!current_section.empty() && !key.empty())
            {
                config_data_[current_section][key] = value;
            }
        }
    }

    std::string Config::trim(const std::string &str)
    {
        if (str.empty())
        {
            return str;
        }

        size_t start = 0;
        size_t end = str.length();

        // Find first non-whitespace
        while (start < end && std::isspace(static_cast<unsigned char>(str[start])))
        {
            start++;
        }

        // Find last non-whitespace
        while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1])))
        {
            end--;
        }

        return str.substr(start, end - start);
    }

    std::string Config::toLower(const std::string &str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    std::optional<std::string> Config::getEnvVar(const std::string &section,
                                                 const std::string &key) const
    {
        // Format: SCRATCHBIRD_SECTION_KEY
        std::string env_name = "SCRATCHBIRD_" + section + "_" + key;

        // Convert to uppercase
        std::transform(env_name.begin(), env_name.end(), env_name.begin(),
                       [](unsigned char c) { return std::toupper(c); });

        const char *value = std::getenv(env_name.c_str());
        if (value != nullptr)
        {
            return std::string(value);
        }

        return std::nullopt;
    }

} // namespace scratchbird::core

namespace scratchbird::core::config
{

    auto catalogKeyDefinitions() -> const std::vector<CatalogKeyDefinition> &
    {
        return catalogDefinitionsStorage();
    }

    auto findCatalogKeyDefinition(std::string_view key_name) -> const CatalogKeyDefinition *
    {
        const std::string normalized = toLowerAsciiCopy(trimAsciiCopy(std::string(key_name)));
        for (const auto &definition : catalogDefinitionsStorage())
        {
            if (normalized == definition.key_name)
            {
                return &definition;
            }
        }
        return nullptr;
    }

    auto isDedicatedTopologyBootstrapKey(std::string_view key_name) -> bool
    {
        const std::string normalized = toLowerAsciiCopy(trimAsciiCopy(std::string(key_name)));
        return dedicatedTopologyKeysStorage().find(normalized) != dedicatedTopologyKeysStorage().end();
    }

    auto validateCatalogValue(const CatalogKeyDefinition &definition,
                              const std::string &raw_value,
                              std::string &normalized_value,
                              std::string &error_message) -> bool
    {
        normalized_value.clear();
        error_message.clear();

        const std::string trimmed = trimAsciiCopy(raw_value);
        if (trimmed.empty())
        {
            normalized_value.clear();
            return true;
        }

        auto validate_numeric_range = [&](auto parsed_value, auto parse_min, auto parse_max) -> bool {
            if (definition.min_value != nullptr)
            {
                try
                {
                    const auto min_value = parse_min(std::string(definition.min_value));
                    if (parsed_value < min_value)
                    {
                        error_message = "Value is below minimum for " + std::string(definition.key_name);
                        return false;
                    }
                }
                catch (...)
                {
                }
            }
            if (definition.max_value != nullptr)
            {
                try
                {
                    const auto max_value = parse_max(std::string(definition.max_value));
                    if (parsed_value > max_value)
                    {
                        error_message = "Value is above maximum for " + std::string(definition.key_name);
                        return false;
                    }
                }
                catch (...)
                {
                }
            }
            return true;
        };

        switch (definition.value_type)
        {
            case CatalogValueType::BOOL:
            {
                const std::string lower = toLowerAsciiCopy(trimmed);
                if (lower == "true" || lower == "yes" || lower == "on" || lower == "1")
                {
                    normalized_value = "true";
                    return true;
                }
                if (lower == "false" || lower == "no" || lower == "off" || lower == "0")
                {
                    normalized_value = "false";
                    return true;
                }
                error_message = "Boolean value required for " + std::string(definition.key_name);
                return false;
            }

            case CatalogValueType::INT64:
            {
                try
                {
                    const int64_t parsed = std::stoll(trimmed);
                    if (!validate_numeric_range(parsed, [](const std::string &v) { return std::stoll(v); },
                                                [](const std::string &v) { return std::stoll(v); }))
                    {
                        return false;
                    }
                    normalized_value = std::to_string(parsed);
                    return true;
                }
                catch (...)
                {
                    error_message = "INT64 value required for " + std::string(definition.key_name);
                    return false;
                }
            }

            case CatalogValueType::UINT64:
            case CatalogValueType::DURATION:
            case CatalogValueType::BYTES:
            {
                try
                {
                    const uint64_t parsed = std::stoull(trimmed);
                    if (!validate_numeric_range(parsed, [](const std::string &v) { return std::stoull(v); },
                                                [](const std::string &v) { return std::stoull(v); }))
                    {
                        return false;
                    }
                    normalized_value = std::to_string(parsed);
                    return true;
                }
                catch (...)
                {
                    error_message = "UINT64 value required for " + std::string(definition.key_name);
                    return false;
                }
            }

            case CatalogValueType::FLOAT64:
            {
                try
                {
                    const double parsed = std::stod(trimmed);
                    if (!std::isfinite(parsed))
                    {
                        error_message = "Finite FLOAT64 value required for " + std::string(definition.key_name);
                        return false;
                    }
                    normalized_value = trimmed;
                    return true;
                }
                catch (...)
                {
                    error_message = "FLOAT64 value required for " + std::string(definition.key_name);
                    return false;
                }
            }

            case CatalogValueType::LIST_STRING:
            {
                std::vector<std::string> input_values = splitCommaList(trimmed);
                if (input_values.empty())
                {
                    normalized_value.clear();
                    return true;
                }

                std::unordered_set<std::string> allowed;
                if (definition.allowed_values != nullptr)
                {
                    for (const auto &entry : splitCommaList(definition.allowed_values))
                    {
                        allowed.insert(toLowerAsciiCopy(entry));
                    }
                }

                std::ostringstream out;
                for (size_t i = 0; i < input_values.size(); ++i)
                {
                    const std::string normalized_item = toLowerAsciiCopy(input_values[i]);
                    if (!allowed.empty() && allowed.find(normalized_item) == allowed.end())
                    {
                        error_message = "Unsupported list value '" + input_values[i] + "' for " +
                                        std::string(definition.key_name);
                        return false;
                    }
                    if (i > 0)
                    {
                        out << ",";
                    }
                    out << normalized_item;
                }
                normalized_value = out.str();
                return true;
            }

            case CatalogValueType::LIST_KV:
            case CatalogValueType::STRING:
            default:
                normalized_value = trimmed;
                break;
        }

        if (definition.allowed_values != nullptr)
        {
            const std::string lower = toLowerAsciiCopy(normalized_value);
            bool matched = false;
            for (const auto &allowed : splitCommaList(definition.allowed_values))
            {
                if (lower == toLowerAsciiCopy(allowed))
                {
                    matched = true;
                    normalized_value = toLowerAsciiCopy(allowed);
                    break;
                }
            }
            if (!matched)
            {
                error_message = "Unsupported value for " + std::string(definition.key_name);
                return false;
            }
        }

        return true;
    }

} // namespace scratchbird::core

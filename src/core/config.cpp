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

#include "scratchbird/core/config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstdlib>

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

        auto val = getValue(section, key);
        return val.has_value() ? val.value() : default_value;
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
        return getValue(section, key).has_value();
    }

    void Config::set(const std::string &section, const std::string &key, const std::string &value)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_data_[section][key] = value;
    }

    std::vector<std::string> Config::getKeys(const std::string &section) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> keys;

        auto it = config_data_.find(section);
        if (it != config_data_.end())
        {
            for (const auto &kv : it->second)
            {
                keys.push_back(kv.first);
            }
        }

        return keys;
    }

    std::vector<std::string> Config::getSections() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> sections;

        for (const auto &section : config_data_)
        {
            sections.push_back(section.first);
        }

        return sections;
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
        cmdline_data_.clear();
        config_file_path_.clear();
        loaded_ = false;
    }

    // Private methods

    std::optional<std::string> Config::getValue(const std::string &section,
                                                const std::string &key) const
    {
        // Priority 1: Command-line arguments
        auto cmd_section = cmdline_data_.find(section);
        if (cmd_section != cmdline_data_.end())
        {
            auto cmd_key = cmd_section->second.find(key);
            if (cmd_key != cmd_section->second.end())
            {
                return cmd_key->second;
            }
        }

        // Priority 2: Environment variables
        auto env_val = getEnvVar(section, key);
        if (env_val.has_value())
        {
            return env_val;
        }

        // Priority 3: Config file
        auto section_it = config_data_.find(section);
        if (section_it != config_data_.end())
        {
            auto key_it = section_it->second.find(key);
            if (key_it != section_it->second.end())
            {
                return key_it->second;
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

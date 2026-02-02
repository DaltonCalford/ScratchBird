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

#include <cstdint>
#include <string>
#include <unordered_map>
#include <optional>
#include <mutex>
#include <vector>
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird::core::config
{
    // Configuration Constants
    // These constants provide default values used throughout the codebase.
    // They can be overridden via Config::getInstance() from sb_config.ini.

    // ===== System Configuration =====

    // Buffer Pool configuration
    constexpr uint32_t DEFAULT_BUFFER_POOL_SIZE = 128; // in pages

    // Heap scan starting page
    constexpr uint32_t HEAP_SCAN_START_PAGE = 7;

    // Number of base schemas
    constexpr int NUM_BASE_SCHEMAS = 8;

    // Maximum number of concurrent backend connections
    constexpr uint32_t DEFAULT_MAX_BACKENDS = 100;

    // Maximum number of locks in the lock manager
    constexpr uint32_t DEFAULT_MAX_LOCKS = 10000;

    // Maximum number of cached transactions in TransactionManager
    constexpr uint32_t DEFAULT_TRANSACTION_CACHE_SIZE = 10000;

    // ===== Timeout Configuration =====

    // Deadlock detection timeout in milliseconds
    constexpr uint32_t DEFAULT_DEADLOCK_TIMEOUT_MS = 1000;

    // Lock acquisition timeout in seconds
    constexpr uint32_t DEFAULT_LOCK_TIMEOUT_SECONDS = 60;

    // Dormant transaction lease in seconds (0 = no automatic expiry)
    constexpr uint32_t DEFAULT_DORMANT_TXN_LEASE_SECONDS = 3600;

    // ===== Transaction/MVCC Configuration =====

    // Initial transaction ID (start value for new databases)
    constexpr uint64_t DEFAULT_INITIAL_XID = 100;

    // Update database header every N transactions
    constexpr uint32_t DEFAULT_HEADER_UPDATE_FREQUENCY = 100;

    // Trigger sweep when this many transactions have passed
    constexpr uint32_t DEFAULT_SWEEP_INTERVAL = 20000;

    // Maximum length of MVCC version chain before breaking
    constexpr uint32_t DEFAULT_MAX_VERSION_CHAIN_LENGTH = 100;

    // ===== Storage/Index Configuration =====

    // Hash index bucket fill threshold (percentage)
    constexpr uint32_t DEFAULT_HASH_BUCKET_FILL_THRESHOLD = 90;

    // B-tree page merge threshold (percentage)
    constexpr uint32_t DEFAULT_BTREE_MERGE_THRESHOLD = 80;

    // TOAST compression threshold (bytes)
    constexpr uint32_t DEFAULT_COMPRESSION_THRESHOLD = 256;

    // Page compression threshold (ratio, 0.0-1.0)
    constexpr double DEFAULT_PAGE_COMPRESSION_THRESHOLD = 0.5;

    // ===== Internal Buffer Sizes (typically not user-configurable) =====

    // Error message buffer size
    constexpr size_t ERROR_MESSAGE_BUFFER_SIZE = 256;

    // Log buffer size
    constexpr size_t LOG_BUFFER_SIZE = 4096;

    // Key output truncation limit
    constexpr size_t KEY_OUTPUT_LIMIT = 256;

} // namespace scratchbird::core::config

namespace scratchbird::core
{

    /**
     * Configuration management singleton
     * Reads configuration from INI file, command-line arguments, and environment variables
     *
     * Priority order (highest to lowest):
     * 1. Command-line arguments (--config-key=value)
     * 2. Environment variables (SCRATCHBIRD_SECTION_KEY)
     * 3. Configuration file (sb_config.ini)
     * 4. Default values
     *
     * Thread-safe: All operations are protected by mutex
     *
     * Usage:
     * @code
     *   Config& cfg = Config::getInstance();
     *   cfg.initialize("sb_config.ini");
     *   int pool_size = cfg.getInt("memory", "buffer_pool_size", 128);
     * @endcode
     */
    class Config
    {
    public:
        // Get singleton instance
        static Config &getInstance();

        /**
         * Initialize configuration from file
         * @param config_file Path to INI file (default: sb_config.ini)
         * @param ctx Error context (optional, can be nullptr)
         * @return Status::OK on success
         */
        Status initialize(const std::string &config_file = "sb_config.ini",
                          ErrorContext *ctx = nullptr);

        /**
         * Load configuration from specific file
         * @param config_file Path to INI file
         * @param ctx Error context (optional, can be nullptr)
         * @return Status::OK on success
         */
        Status loadFile(const std::string &config_file, ErrorContext *ctx = nullptr);

        /**
         * Add command-line argument override
         * @param section Configuration section name
         * @param key Configuration key name
         * @param value Value to set
         */
        void addCommandLineArg(const std::string &section, const std::string &key,
                               const std::string &value);

        /**
         * Get configuration value as string
         * @param section Configuration section
         * @param key Configuration key
         * @param default_value Default if not found
         * @return Configuration value or default
         */
        std::string getString(const std::string &section, const std::string &key,
                              const std::string &default_value = "") const;

        /**
         * Get configuration value as integer
         * @param section Configuration section
         * @param key Configuration key
         * @param default_value Default if not found
         * @return Configuration value or default
         */
        int64_t getInt(const std::string &section, const std::string &key,
                       int64_t default_value = 0) const;

        /**
         * Get configuration value as unsigned integer
         * @param section Configuration section
         * @param key Configuration key
         * @param default_value Default if not found
         * @return Configuration value or default
         */
        uint64_t getUInt(const std::string &section, const std::string &key,
                         uint64_t default_value = 0) const;

        /**
         * Get configuration value as boolean
         * Recognizes: true/false, yes/no, on/off, 1/0 (case-insensitive)
         * @param section Configuration section
         * @param key Configuration key
         * @param default_value Default if not found
         * @return Configuration value or default
         */
        bool getBool(const std::string &section, const std::string &key,
                     bool default_value = false) const;

        /**
         * Get configuration value as double
         * @param section Configuration section
         * @param key Configuration key
         * @param default_value Default if not found
         * @return Configuration value or default
         */
        double getDouble(const std::string &section, const std::string &key,
                         double default_value = 0.0) const;

        /**
         * Check if key exists in configuration
         * @param section Configuration section
         * @param key Configuration key
         * @return true if key exists
         */
        bool hasKey(const std::string &section, const std::string &key) const;

        /**
         * Set value programmatically (for testing or runtime changes)
         * @param section Configuration section
         * @param key Configuration key
         * @param value Value to set
         */
        void set(const std::string &section, const std::string &key, const std::string &value);

        /**
         * Get all keys in a section
         * @param section Configuration section
         * @return Vector of key names
         */
        std::vector<std::string> getKeys(const std::string &section) const;

        /**
         * Get all sections
         * @return Vector of section names
         */
        std::vector<std::string> getSections() const;

        /**
         * Reload configuration from file
         * @param ctx Error context (optional, can be nullptr)
         * @return Status::OK on success
         */
        Status reload(ErrorContext *ctx = nullptr);

        /**
         * Clear all configuration
         */
        void clear();

        /**
         * Check if configuration is loaded
         * @return true if initialized
         */
        bool isLoaded() const
        {
            return loaded_;
        }

        /**
         * Get config file path
         * @return Path to loaded config file
         */
        std::string getConfigFilePath() const
        {
            return config_file_path_;
        }

    private:
        Config() = default;
        ~Config() = default;

        // Delete copy and move
        Config(const Config &) = delete;
        Config &operator=(const Config &) = delete;
        Config(Config &&) = delete;
        Config &operator=(Config &&) = delete;

        // Internal helper to get value with priority
        std::optional<std::string> getValue(const std::string &section,
                                            const std::string &key) const;

        // Parse INI file
        Status parseFile(const std::string &filepath, ErrorContext *ctx);

        // Parse INI line
        void parseLine(const std::string &line, std::string &current_section);

        // Trim whitespace from string
        static std::string trim(const std::string &str);

        // To lowercase for case-insensitive comparison
        static std::string toLower(const std::string &str);

        // Check environment variable (SCRATCHBIRD_SECTION_KEY format)
        std::optional<std::string> getEnvVar(const std::string &section,
                                             const std::string &key) const;

        // Storage structure: section -> key -> value
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> config_data_;

        // Command-line overrides (highest priority)
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> cmdline_data_;

        // Mutex for thread-safety
        mutable std::mutex mutex_;

        // Configuration file path
        std::string config_file_path_;

        // Loaded flag
        bool loaded_ = false;
    };

// Convenience macro for accessing singleton
#define CONFIG Config::getInstance()

} // namespace scratchbird::core

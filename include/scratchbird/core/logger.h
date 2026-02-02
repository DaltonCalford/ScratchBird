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

#include <string>
#include <mutex>
#include <fstream>
#include <cstdint>
#include <memory>

namespace scratchbird::core
{

    /**
     * Log levels
     */
    enum class LogLevel : uint8_t
    {
        TRACE = 0,   // Very detailed debug information
        DEBUG = 1,   // Debug information
        INFO = 2,    // Informational messages
        WARNING = 3, // Warning messages
        ERROR = 4,   // Error messages
        CRITICAL = 5 // Critical errors
    };

    /**
     * Log categories for filtering and organization
     */
    enum class LogCategory : uint8_t
    {
        GENERAL = 0,
        STORAGE = 1,
        TRANSACTION = 2,
        LOCK = 3,
        PARSER = 4,
        EXECUTOR = 5,
        NETWORK = 6,
        CATALOG = 7,
        BTREE = 8,
        HASH = 9,
        BUFFER = 10,
        VACUUM = 11
    };

    /**
     * Thread-safe logging framework
     * Replaces fprintf(stderr) calls throughout codebase
     *
     * Usage:
     * @code
     *   Logger& log = Logger::getInstance();
     *   log.initialize();
     *   LOG_INFO(STORAGE, "Database opened: %s", db_name.c_str());
     *   LOG_ERROR(TRANSACTION, "Transaction %lu failed", xid);
     * @endcode
     */
    class Logger
    {
    public:
        /**
         * Get singleton instance
         */
        static Logger &getInstance();

        /**
         * Initialize logger with configuration
         * Reads settings from Config if available, otherwise uses defaults
         */
        void initialize();

        /**
         * Set log level
         * Only messages at this level or higher will be logged
         */
        void setLogLevel(LogLevel level);

        /**
         * Get current log level
         */
        LogLevel getLogLevel() const
        {
            return log_level_;
        }

        /**
         * Set log output file
         * Empty string means stderr
         */
        void setLogFile(const std::string &filepath);

        /**
         * Enable/disable timestamp in log messages
         */
        void setTimestampEnabled(bool enabled)
        {
            timestamp_enabled_ = enabled;
        }

        /**
         * Enable/disable thread ID in log messages
         */
        void setThreadIdEnabled(bool enabled)
        {
            thread_id_enabled_ = enabled;
        }

        /**
         * Enable/disable source location in log messages
         */
        void setSourceLocationEnabled(bool enabled)
        {
            source_location_enabled_ = enabled;
        }

        /**
         * Log a message
         */
        void log(LogLevel level, LogCategory category, const char *file, int line,
                 const char *function, const char *format, ...);

        /**
         * Flush log output
         */
        void flush();

        /**
         * Close log file
         */
        void close();

        /**
         * Convert log level to string
         */
        static const char *levelToString(LogLevel level);

        /**
         * Convert log category to string
         */
        static const char *categoryToString(LogCategory category);

    private:
        Logger() = default;
        ~Logger();

        // Delete copy and move
        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;
        Logger(Logger &&) = delete;
        Logger &operator=(Logger &&) = delete;

        // Internal log implementation
        void logInternal(LogLevel level, LogCategory category, const char *file, int line,
                         const char *function, const char *message);

        // Get current timestamp string
        std::string getTimestamp();

        // Get current thread ID
        uint64_t getThreadId();

        // Mutex for thread-safety
        mutable std::mutex mutex_;

        // Log level filter
        LogLevel log_level_ = LogLevel::INFO;

        // Output file stream (nullptr means stderr)
        std::unique_ptr<std::ofstream> log_file_;

        // Log file path
        std::string log_file_path_;

        // Configuration flags
        bool timestamp_enabled_ = true;
        bool thread_id_enabled_ = false;
        bool source_location_enabled_ = true;
        bool initialized_ = false;
    };

    // Convenience macros for logging

#define LOG_TRACE(category, format, ...)                                                  \
    Logger::getInstance().log(LogLevel::TRACE, LogCategory::category, __FILE__, __LINE__, \
                              __func__, format, ##__VA_ARGS__)

#define LOG_DEBUG(category, format, ...)                                                  \
    Logger::getInstance().log(LogLevel::DEBUG, LogCategory::category, __FILE__, __LINE__, \
                              __func__, format, ##__VA_ARGS__)

#define LOG_INFO(category, format, ...)                                                            \
    Logger::getInstance().log(LogLevel::INFO, LogCategory::category, __FILE__, __LINE__, __func__, \
                              format, ##__VA_ARGS__)

#define LOG_WARNING(category, format, ...)                                                  \
    Logger::getInstance().log(LogLevel::WARNING, LogCategory::category, __FILE__, __LINE__, \
                              __func__, format, ##__VA_ARGS__)

#define LOG_ERROR(category, format, ...)                                                  \
    Logger::getInstance().log(LogLevel::ERROR, LogCategory::category, __FILE__, __LINE__, \
                              __func__, format, ##__VA_ARGS__)

#define LOG_CRITICAL(category, format, ...)                                                  \
    Logger::getInstance().log(LogLevel::CRITICAL, LogCategory::category, __FILE__, __LINE__, \
                              __func__, format, ##__VA_ARGS__)

} // namespace scratchbird::core

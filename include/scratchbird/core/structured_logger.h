/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P3-17: Structured Logging
//
// JSON-based structured logging for machine-parseable log output.
// Integrates with existing Logger infrastructure while providing structured output.
//
// November 25, 2025

#pragma once

#include "scratchbird/core/logger.h"
#include "scratchbird/core/status.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <chrono>
#include <mutex>
#include <memory>
#include <functional>

namespace scratchbird::core {

// Forward declarations
class Database;

// Structured log entry with key-value pairs
class StructuredLogEntry {
public:
    StructuredLogEntry(LogLevel level, LogCategory category);

    // Fluent interface for adding fields
    StructuredLogEntry& field(const std::string& key, const std::string& value);
    StructuredLogEntry& field(const std::string& key, int64_t value);
    StructuredLogEntry& field(const std::string& key, uint64_t value);
    StructuredLogEntry& field(const std::string& key, double value);
    StructuredLogEntry& field(const std::string& key, bool value);

    // Add message
    StructuredLogEntry& message(const std::string& msg);

    // Add error context
    StructuredLogEntry& error(const std::string& error_code, const std::string& error_msg);

    // Add timing information
    StructuredLogEntry& duration_ms(double ms);
    StructuredLogEntry& duration_us(double us);

    // Add source location
    StructuredLogEntry& location(const char* file, int line, const char* function);

    // Convert to JSON string
    std::string toJson() const;

    // Getters
    LogLevel level() const { return level_; }
    LogCategory category() const { return category_; }

    // JSON escaping helper - public static for use by related log entry types
    static std::string escapeJson(const std::string& str);

private:
    LogLevel level_;
    LogCategory category_;
    std::string message_;
    std::unordered_map<std::string, std::string> string_fields_;
    std::unordered_map<std::string, int64_t> int_fields_;
    std::unordered_map<std::string, uint64_t> uint_fields_;
    std::unordered_map<std::string, double> double_fields_;
    std::unordered_map<std::string, bool> bool_fields_;
    std::string error_code_;
    std::string error_message_;
    std::string file_;
    int line_ = 0;
    std::string function_;
    std::chrono::system_clock::time_point timestamp_;
};

// Structured logger configuration
struct StructuredLoggerConfig {
    bool enabled = false;                    // Enable structured logging
    bool include_timestamp = true;           // Include ISO8601 timestamp
    bool include_level = true;               // Include log level
    bool include_category = true;            // Include log category
    bool include_thread_id = true;           // Include thread ID
    bool include_source_location = false;    // Include file/line/function
    bool pretty_print = false;               // Pretty-print JSON (newlines, indentation)
    std::string output_file;                 // Output file path (empty = stderr)
};

// Output handler for structured logs
using StructuredLogHandler = std::function<void(const std::string& json_log)>;

// Structured logger singleton
class StructuredLogger {
public:
    static StructuredLogger& getInstance();

    // Initialize with configuration
    void initialize(const StructuredLoggerConfig& config);

    // Check if structured logging is enabled
    bool isEnabled() const { return config_.enabled; }

    // Get/set configuration
    const StructuredLoggerConfig& config() const { return config_; }
    void setConfig(const StructuredLoggerConfig& config);

    // Log a structured entry
    void log(const StructuredLogEntry& entry);

    // Create a new log entry
    StructuredLogEntry entry(LogLevel level, LogCategory category);

    // Register custom output handler (e.g., for sending to log aggregator)
    void setOutputHandler(StructuredLogHandler handler);

    // Flush output
    void flush();

    // Close output file
    void close();

private:
    StructuredLogger() = default;
    ~StructuredLogger();

    StructuredLogger(const StructuredLogger&) = delete;
    StructuredLogger& operator=(const StructuredLogger&) = delete;

    void writeLog(const std::string& json);

    mutable std::mutex mutex_;
    StructuredLoggerConfig config_;
    std::unique_ptr<std::ofstream> output_file_;
    StructuredLogHandler custom_handler_;
    bool initialized_ = false;
};

// Convenience macros for structured logging
#define SLOG_TRACE(category) \
    if (StructuredLogger::getInstance().isEnabled()) \
        StructuredLogger::getInstance().log( \
            StructuredLogger::getInstance().entry(LogLevel::TRACE, LogCategory::category) \
                .location(__FILE__, __LINE__, __func__)

#define SLOG_DEBUG(category) \
    if (StructuredLogger::getInstance().isEnabled()) \
        StructuredLogger::getInstance().log( \
            StructuredLogger::getInstance().entry(LogLevel::DEBUG, LogCategory::category) \
                .location(__FILE__, __LINE__, __func__)

#define SLOG_INFO(category) \
    if (StructuredLogger::getInstance().isEnabled()) \
        StructuredLogger::getInstance().log( \
            StructuredLogger::getInstance().entry(LogLevel::INFO, LogCategory::category) \
                .location(__FILE__, __LINE__, __func__)

#define SLOG_WARNING(category) \
    if (StructuredLogger::getInstance().isEnabled()) \
        StructuredLogger::getInstance().log( \
            StructuredLogger::getInstance().entry(LogLevel::WARNING, LogCategory::category) \
                .location(__FILE__, __LINE__, __func__)

#define SLOG_ERROR(category) \
    if (StructuredLogger::getInstance().isEnabled()) \
        StructuredLogger::getInstance().log( \
            StructuredLogger::getInstance().entry(LogLevel::ERROR, LogCategory::category) \
                .location(__FILE__, __LINE__, __func__)

#define SLOG_CRITICAL(category) \
    if (StructuredLogger::getInstance().isEnabled()) \
        StructuredLogger::getInstance().log( \
            StructuredLogger::getInstance().entry(LogLevel::CRITICAL, LogCategory::category) \
                .location(__FILE__, __LINE__, __func__)

// Query-specific structured logging
struct QueryLogEntry {
    std::string query_id;
    std::string sql_text;
    std::string user;
    std::string database;
    uint64_t start_time_us;
    uint64_t end_time_us;
    uint64_t rows_returned;
    uint64_t rows_examined;
    bool success;
    std::string error_code;
    std::string error_message;
    std::string plan_summary;

    std::string toJson() const;
};

// Transaction-specific structured logging
struct TransactionLogEntry {
    uint64_t xid;
    std::string state;      // "begin", "commit", "rollback", "prepared"
    uint64_t start_time_us;
    uint64_t end_time_us;
    uint32_t num_statements;
    uint64_t rows_modified;
    std::string user;
    std::string database;

    std::string toJson() const;
};

// Lock wait structured logging
struct LockWaitLogEntry {
    uint64_t waiting_xid;
    uint64_t blocking_xid;
    std::string lock_type;
    std::string resource;
    uint64_t wait_time_us;
    bool acquired;
    bool deadlock;

    std::string toJson() const;
};

} // namespace scratchbird::core

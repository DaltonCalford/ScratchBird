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
// P3-17: Structured Logging Implementation
//
// November 25, 2025

#include "scratchbird/core/structured_logger.h"
#include <fstream>
#include <iomanip>
#include <thread>
#include <ctime>
#include <iostream>

namespace scratchbird::core {

// ============================================================================
// StructuredLogEntry Implementation
// ============================================================================

StructuredLogEntry::StructuredLogEntry(LogLevel level, LogCategory category)
    : level_(level), category_(category), timestamp_(std::chrono::system_clock::now()) {
}

StructuredLogEntry& StructuredLogEntry::field(const std::string& key, const std::string& value) {
    string_fields_[key] = value;
    return *this;
}

StructuredLogEntry& StructuredLogEntry::field(const std::string& key, int64_t value) {
    int_fields_[key] = value;
    return *this;
}

StructuredLogEntry& StructuredLogEntry::field(const std::string& key, uint64_t value) {
    uint_fields_[key] = value;
    return *this;
}

StructuredLogEntry& StructuredLogEntry::field(const std::string& key, double value) {
    double_fields_[key] = value;
    return *this;
}

StructuredLogEntry& StructuredLogEntry::field(const std::string& key, bool value) {
    bool_fields_[key] = value;
    return *this;
}

StructuredLogEntry& StructuredLogEntry::message(const std::string& msg) {
    message_ = msg;
    return *this;
}

StructuredLogEntry& StructuredLogEntry::error(const std::string& error_code, const std::string& error_msg) {
    error_code_ = error_code;
    error_message_ = error_msg;
    return *this;
}

StructuredLogEntry& StructuredLogEntry::duration_ms(double ms) {
    double_fields_["duration_ms"] = ms;
    return *this;
}

StructuredLogEntry& StructuredLogEntry::duration_us(double us) {
    double_fields_["duration_us"] = us;
    return *this;
}

StructuredLogEntry& StructuredLogEntry::location(const char* file, int line, const char* function) {
    file_ = file ? file : "";
    line_ = line;
    function_ = function ? function : "";
    return *this;
}

std::string StructuredLogEntry::escapeJson(const std::string& str) {
    std::ostringstream ss;
    for (char c : str) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    ss << c;
                }
                break;
        }
    }
    return ss.str();
}

std::string StructuredLogEntry::toJson() const {
    std::ostringstream ss;
    ss << "{";

    bool first = true;

    // Timestamp (ISO8601)
    auto time_t = std::chrono::system_clock::to_time_t(timestamp_);
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        timestamp_.time_since_epoch()).count() % 1000000;
    std::tm tm_buf;
    gmtime_r(&time_t, &tm_buf);
    ss << "\"timestamp\":\"" << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
       << "." << std::setw(6) << std::setfill('0') << us << "Z\"";
    first = false;

    // Level
    if (!first) ss << ",";
    ss << "\"level\":\"" << Logger::levelToString(level_) << "\"";

    // Category
    ss << ",\"category\":\"" << Logger::categoryToString(category_) << "\"";

    // Thread ID
    ss << ",\"thread_id\":" << std::hash<std::thread::id>{}(std::this_thread::get_id());

    // Message
    if (!message_.empty()) {
        ss << ",\"message\":\"" << escapeJson(message_) << "\"";
    }

    // Error info
    if (!error_code_.empty()) {
        ss << ",\"error\":{\"code\":\"" << escapeJson(error_code_) << "\",\"message\":\""
           << escapeJson(error_message_) << "\"}";
    }

    // Source location
    if (!file_.empty()) {
        ss << ",\"source\":{\"file\":\"" << escapeJson(file_) << "\",\"line\":" << line_
           << ",\"function\":\"" << escapeJson(function_) << "\"}";
    }

    // Custom fields
    for (const auto& [key, value] : string_fields_) {
        ss << ",\"" << escapeJson(key) << "\":\"" << escapeJson(value) << "\"";
    }

    for (const auto& [key, value] : int_fields_) {
        ss << ",\"" << escapeJson(key) << "\":" << value;
    }

    for (const auto& [key, value] : uint_fields_) {
        ss << ",\"" << escapeJson(key) << "\":" << value;
    }

    for (const auto& [key, value] : double_fields_) {
        ss << ",\"" << escapeJson(key) << "\":" << std::fixed << std::setprecision(6) << value;
    }

    for (const auto& [key, value] : bool_fields_) {
        ss << ",\"" << escapeJson(key) << "\":" << (value ? "true" : "false");
    }

    ss << "}";
    return ss.str();
}

// ============================================================================
// StructuredLogger Implementation
// ============================================================================

StructuredLogger& StructuredLogger::getInstance() {
    static StructuredLogger instance;
    return instance;
}

StructuredLogger::~StructuredLogger() {
    close();
}

void StructuredLogger::initialize(const StructuredLoggerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;

    if (!config_.output_file.empty()) {
        output_file_ = std::make_unique<std::ofstream>(config_.output_file, std::ios::app);
        if (!output_file_->is_open()) {
            output_file_.reset();
        }
    }

    initialized_ = true;
}

void StructuredLogger::setConfig(const StructuredLoggerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

void StructuredLogger::log(const StructuredLogEntry& entry) {
    if (!config_.enabled) return;

    std::string json = entry.toJson();

    std::lock_guard<std::mutex> lock(mutex_);
    writeLog(json);
}

StructuredLogEntry StructuredLogger::entry(LogLevel level, LogCategory category) {
    return StructuredLogEntry(level, category);
}

void StructuredLogger::setOutputHandler(StructuredLogHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    custom_handler_ = std::move(handler);
}

void StructuredLogger::writeLog(const std::string& json) {
    // Custom handler first
    if (custom_handler_) {
        custom_handler_(json);
    }

    // Then file output
    if (output_file_ && output_file_->is_open()) {
        *output_file_ << json << "\n";
    } else if (config_.output_file.empty()) {
        // stderr output
        std::cerr << json << "\n";
    }
}

void StructuredLogger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (output_file_ && output_file_->is_open()) {
        output_file_->flush();
    }
}

void StructuredLogger::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (output_file_) {
        output_file_->close();
        output_file_.reset();
    }
}

// ============================================================================
// QueryLogEntry Implementation
// ============================================================================

std::string QueryLogEntry::toJson() const {
    std::ostringstream ss;
    ss << "{\"type\":\"query\"";
    ss << ",\"query_id\":\"" << StructuredLogEntry::escapeJson(query_id) << "\"";
    if (!sql_text.empty()) {
        ss << ",\"sql\":\"" << StructuredLogEntry::escapeJson(sql_text) << "\"";
    }
    ss << ",\"user\":\"" << StructuredLogEntry::escapeJson(user) << "\"";
    ss << ",\"database\":\"" << StructuredLogEntry::escapeJson(database) << "\"";
    ss << ",\"start_time_us\":" << start_time_us;
    ss << ",\"end_time_us\":" << end_time_us;
    ss << ",\"duration_us\":" << (end_time_us - start_time_us);
    ss << ",\"rows_returned\":" << rows_returned;
    ss << ",\"rows_examined\":" << rows_examined;
    ss << ",\"success\":" << (success ? "true" : "false");
    if (!success && !error_code.empty()) {
        ss << ",\"error\":{\"code\":\"" << StructuredLogEntry::escapeJson(error_code)
           << "\",\"message\":\"" << StructuredLogEntry::escapeJson(error_message) << "\"}";
    }
    if (!plan_summary.empty()) {
        ss << ",\"plan\":\"" << StructuredLogEntry::escapeJson(plan_summary) << "\"";
    }
    ss << "}";
    return ss.str();
}

// ============================================================================
// TransactionLogEntry Implementation
// ============================================================================

std::string TransactionLogEntry::toJson() const {
    std::ostringstream ss;
    ss << "{\"type\":\"transaction\"";
    ss << ",\"xid\":" << xid;
    ss << ",\"state\":\"" << StructuredLogEntry::escapeJson(state) << "\"";
    ss << ",\"start_time_us\":" << start_time_us;
    ss << ",\"end_time_us\":" << end_time_us;
    ss << ",\"duration_us\":" << (end_time_us - start_time_us);
    ss << ",\"num_statements\":" << num_statements;
    ss << ",\"rows_modified\":" << rows_modified;
    ss << ",\"user\":\"" << StructuredLogEntry::escapeJson(user) << "\"";
    ss << ",\"database\":\"" << StructuredLogEntry::escapeJson(database) << "\"";
    ss << "}";
    return ss.str();
}

// ============================================================================
// LockWaitLogEntry Implementation
// ============================================================================

std::string LockWaitLogEntry::toJson() const {
    std::ostringstream ss;
    ss << "{\"type\":\"lock_wait\"";
    ss << ",\"waiting_xid\":" << waiting_xid;
    ss << ",\"blocking_xid\":" << blocking_xid;
    ss << ",\"lock_type\":\"" << StructuredLogEntry::escapeJson(lock_type) << "\"";
    ss << ",\"resource\":\"" << StructuredLogEntry::escapeJson(resource) << "\"";
    ss << ",\"wait_time_us\":" << wait_time_us;
    ss << ",\"acquired\":" << (acquired ? "true" : "false");
    ss << ",\"deadlock\":" << (deadlock ? "true" : "false");
    ss << "}";
    return ss.str();
}

} // namespace scratchbird::core

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
 * Logger Implementation
 * Thread-safe logging framework with multiple log levels and categories
 * Phase 1: Foundation Infrastructure
 */

#include "scratchbird/core/logger.h"
#include "scratchbird/core/config.h"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace scratchbird::core
{

    Logger &Logger::getInstance()
    {
        static Logger instance;
        return instance;
    }

    Logger::~Logger()
    {
        close();
    }

    void Logger::initialize()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (initialized_)
        {
            return;
        }

        // Try to read from config if available
        Config &cfg = Config::getInstance();
        if (cfg.isLoaded())
        {
            // Read log level
            std::string level_str = cfg.getString("logging", "log_level", "INFO");
            if (level_str == "TRACE")
                log_level_ = LogLevel::TRACE;
            else if (level_str == "DEBUG")
                log_level_ = LogLevel::DEBUG;
            else if (level_str == "INFO")
                log_level_ = LogLevel::INFO;
            else if (level_str == "WARNING")
                log_level_ = LogLevel::WARNING;
            else if (level_str == "ERROR")
                log_level_ = LogLevel::ERROR;
            else if (level_str == "CRITICAL")
                log_level_ = LogLevel::CRITICAL;

            // Read log file
            std::string log_file = cfg.getString("logging", "log_file", "");
            if (!log_file.empty())
            {
                setLogFile(log_file);
            }

            // Read configuration flags
            timestamp_enabled_ = cfg.getBool("logging", "log_timestamp", true);
            thread_id_enabled_ = cfg.getBool("logging", "log_thread_id", false);
            source_location_enabled_ = cfg.getBool("logging", "log_source_location", true);
        }

        initialized_ = true;
    }

    void Logger::setLogLevel(LogLevel level)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        log_level_ = level;
    }

    void Logger::setLogFile(const std::string &filepath)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Close existing file if open
        if (log_file_ && log_file_->is_open())
        {
            log_file_->close();
        }

        if (filepath.empty())
        {
            log_file_.reset();
            log_file_path_.clear();
            return;
        }

        // Open new file
        log_file_ = std::make_unique<std::ofstream>(filepath, std::ios::app);
        if (!log_file_->is_open())
        {
            fprintf(stderr, "[Logger] Failed to open log file: %s\n", filepath.c_str());
            log_file_.reset();
        }
        else
        {
            log_file_path_ = filepath;
        }
    }

    void Logger::log(LogLevel level, LogCategory category, const char *file, int line,
                     const char *function, const char *format, ...)
    {
        // Filter by log level (without lock for performance)
        if (level < log_level_)
        {
            return;
        }

        // Format the message
        char buffer[4096];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        // Log the formatted message
        logInternal(level, category, file, line, function, buffer);
    }

    void Logger::logInternal(LogLevel level, LogCategory category, const char *file, int line,
                             const char *function, const char *message)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Build log message
        std::ostringstream oss;

        // Timestamp
        if (timestamp_enabled_)
        {
            oss << "[" << getTimestamp() << "] ";
        }

        // Log level
        oss << "[" << levelToString(level) << "] ";

        // Category
        oss << "[" << categoryToString(category) << "] ";

        // Thread ID (optional)
        if (thread_id_enabled_)
        {
            oss << "[Thread:" << getThreadId() << "] ";
        }

        // Source location (optional)
        if (source_location_enabled_ && file != nullptr)
        {
            // Extract filename from full path
            const char *filename = file;
            const char *slash = nullptr;
            for (const char *p = file; *p; ++p)
            {
                if (*p == '/' || *p == '\\')
                {
                    slash = p;
                }
            }
            if (slash)
            {
                filename = slash + 1;
            }

            oss << "[" << filename << ":" << line << "] ";
        }

        // Message
        oss << message;

        // Write to output
        std::string log_line = oss.str();

        if (log_file_ && log_file_->is_open())
        {
            *log_file_ << log_line << std::endl;
        }
        else
        {
            fprintf(stderr, "%s\n", log_line.c_str());
        }
    }

    void Logger::flush()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (log_file_ && log_file_->is_open())
        {
            log_file_->flush();
        }
        else
        {
            fflush(stderr);
        }
    }

    void Logger::close()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (log_file_ && log_file_->is_open())
        {
            log_file_->close();
        }
        log_file_.reset();
    }

    const char *Logger::levelToString(LogLevel level)
    {
        switch (level)
        {
            case LogLevel::TRACE:
                return "TRACE";
            case LogLevel::DEBUG:
                return "DEBUG";
            case LogLevel::INFO:
                return "INFO";
            case LogLevel::WARNING:
                return "WARN";
            case LogLevel::ERROR:
                return "ERROR";
            case LogLevel::CRITICAL:
                return "CRIT";
            default:
                return "UNKNOWN";
        }
    }

    const char *Logger::categoryToString(LogCategory category)
    {
        switch (category)
        {
            case LogCategory::GENERAL:
                return "GENERAL";
            case LogCategory::STORAGE:
                return "STORAGE";
            case LogCategory::TRANSACTION:
                return "TRANSACTION";
            case LogCategory::LOCK:
                return "LOCK";
            case LogCategory::PARSER:
                return "PARSER";
            case LogCategory::EXECUTOR:
                return "EXECUTOR";
            case LogCategory::NETWORK:
                return "NETWORK";
            case LogCategory::CATALOG:
                return "CATALOG";
            case LogCategory::BTREE:
                return "BTREE";
            case LogCategory::HASH:
                return "HASH";
            case LogCategory::BUFFER:
                return "BUFFER";
            case LogCategory::VACUUM:
                return "VACUUM";
            default:
                return "UNKNOWN";
        }
    }

    std::string Logger::getTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
#else
        localtime_r(&time_t, &tm_buf);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
            << std::setw(3) << ms.count();

        return oss.str();
    }

    uint64_t Logger::getThreadId()
    {
        auto id = std::this_thread::get_id();
        std::hash<std::thread::id> hasher;
        return hasher(id);
    }

} // namespace scratchbird::core

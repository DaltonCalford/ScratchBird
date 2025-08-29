// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/telemetry/logging.h"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

#if defined(__unix__) || defined(__APPLE__)
#include <syslog.h>
#endif

namespace fs = std::filesystem;

namespace scratchbird::telemetry
{

    static std::string level_to_string(LogLevel level)
    {
        switch (level) {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
        }
        return "INFO";
    }

    void StdoutSink::write(const LogEvent&, std::string_view formatted)
    {
        static std::mutex stdout_mutex;
        std::lock_guard<std::mutex> lock(stdout_mutex);
        std::fwrite(formatted.data(), 1, formatted.size(), stdout);
        std::fwrite("\n", 1, 1, stdout);
    }

    void StdoutSink::flush()
    {
        std::fflush(stdout);
    }

    FileSink::FileSink(std::string path, std::size_t max_bytes, int rotation_count)
        : file_path_(std::move(path)), max_bytes_(max_bytes), rotation_count_(rotation_count)
    {
        auto out = std::make_unique<std::ofstream>(file_path_, std::ios::app | std::ios::binary);
        if (!*out) {
            // fallback to stdout if file cannot be opened
            stream_ = std::make_unique<std::ostream>(std::cout.rdbuf());
        } else {
            stream_ = std::move(out);
        }
        if (fs::exists(file_path_)) {
            current_size_ = static_cast<std::size_t>(fs::file_size(file_path_));
        }
    }

    FileSink::~FileSink()
    {
        flush();
    }

    void FileSink::write(const LogEvent&, std::string_view formatted)
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        rotate_if_needed(formatted.size() + 1);
        (*stream_) << formatted << '\n';
        current_size_ += formatted.size() + 1;
    }

    void FileSink::flush()
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        stream_->flush();
    }

    void FileSink::rotate_if_needed(std::size_t incoming_bytes)
    {
        if (max_bytes_ == 0)
            return;
        if (current_size_ + incoming_bytes <= max_bytes_)
            return;
        rotate_files();
        // reopen file
        stream_.reset();
        auto out = std::make_unique<std::ofstream>(file_path_, std::ios::trunc | std::ios::binary);
        if (!*out) {
            stream_ = std::make_unique<std::ostream>(std::cout.rdbuf());
        } else {
            stream_ = std::move(out);
        }
        current_size_ = 0;
    }

    void FileSink::rotate_files()
    {
        if (rotation_count_ <= 0)
            return;
        // Remove oldest
        fs::path base(file_path_);
        fs::path oldest = base;
        oldest += "." + std::to_string(rotation_count_);
        std::error_code ec;
        fs::remove(oldest, ec);
        // Shift others
        for (int i = rotation_count_ - 1; i >= 1; --i) {
            fs::path from = base;
            from += "." + std::to_string(i);
            fs::path to = base;
            to += "." + std::to_string(i + 1);
            fs::rename(from, to, ec);
        }
        // Move current to .1
        fs::path to1 = base;
        to1 += ".1";
        fs::rename(base, to1, ec);
    }

    SyslogSink::SyslogSink(std::string ident) : ident_(std::move(ident))
    {
#if defined(__unix__) || defined(__APPLE__)
        openlog(ident_.c_str(), LOG_PID | LOG_NDELAY, LOG_USER);
        available_ = true;
#else
        available_ = false;
#endif
    }

    SyslogSink::~SyslogSink()
    {
#if defined(__unix__) || defined(__APPLE__)
        if (available_)
            closelog();
#endif
    }

    void SyslogSink::write(const LogEvent& event, std::string_view formatted)
    {
#if defined(__unix__) || defined(__APPLE__)
        if (!available_)
            return;
        int priority = LOG_INFO;
        switch (event.level) {
        case LogLevel::Trace:
        case LogLevel::Debug:
            priority = LOG_DEBUG;
            break;
        case LogLevel::Info:
            priority = LOG_INFO;
            break;
        case LogLevel::Warn:
            priority = LOG_WARNING;
            break;
        case LogLevel::Error:
            priority = LOG_ERR;
            break;
        }
        syslog(priority, "%.*s", (int)formatted.size(), formatted.data());
#else
        (void)event;
        (void)formatted;
#endif
    }

    Logger& Logger::instance()
    {
        static Logger inst;
        return inst;
    }

    Logger::Logger() : level_int_(static_cast<int>(LogLevel::Info)), json_format_(false)
    {
        sinks_.push_back(std::make_shared<StdoutSink>());
    }

    void Logger::set_level(LogLevel level)
    {
        level_int_.store(static_cast<int>(level), std::memory_order_relaxed);
    }
    LogLevel Logger::level() const
    {
        return static_cast<LogLevel>(level_int_.load(std::memory_order_relaxed));
    }

    void Logger::set_json_format(bool enabled)
    {
        json_format_.store(enabled, std::memory_order_relaxed);
    }
    bool Logger::json_format() const
    {
        return json_format_.load(std::memory_order_relaxed);
    }

    void Logger::set_sinks(std::vector<std::shared_ptr<LogSink>> sinks)
    {
        std::lock_guard<std::mutex> lock(sinks_mutex_);
        sinks_ = std::move(sinks);
    }

    void Logger::add_sink(std::shared_ptr<LogSink> sink)
    {
        std::lock_guard<std::mutex> lock(sinks_mutex_);
        sinks_.push_back(std::move(sink));
    }

    void Logger::clear_sinks()
    {
        std::lock_guard<std::mutex> lock(sinks_mutex_);
        sinks_.clear();
    }

    uint64_t Logger::current_thread_id()
    {
        auto id = std::hash<std::thread::id>{}(std::this_thread::get_id());
        return static_cast<uint64_t>(id);
    }

    std::string Logger::format_event(const LogEvent& event, bool json)
    {
        using std::chrono::system_clock;
        auto t = system_clock::to_time_t(event.timestamp);
        auto tm = *std::gmtime(&t);
        std::ostringstream oss;
        if (json) {
            oss << "{\"ts\":\"" << std::put_time(&tm, "%FT%TZ") << "\",";
            oss << "\"level\":\"" << level_to_string(event.level) << "\",";
            oss << "\"logger\":\"" << event.logger_name << "\",";
            oss << "\"msg\":\"";
            for (char c : event.message) {
                if (c == '"')
                    oss << "\\\"";
                else if (c == '\\')
                    oss << "\\\\";
                else if (c == '\n')
                    oss << "\\n";
                else
                    oss << c;
            }
            oss << "\",";
            oss << "\"file\":\"" << event.file << "\",";
            oss << "\"line\":" << event.line << ",";
            oss << "\"tid\":" << event.thread_id << "}";
        } else {
            oss << std::put_time(&tm, "%FT%TZ") << " [" << level_to_string(event.level) << "] "
                << event.logger_name << ": " << event.message << " (" << event.file << ":"
                << event.line << ")"
                << " tid=" << event.thread_id;
        }
        return oss.str();
    }

    void Logger::log(LogLevel level, std::string logger_name, std::string message, std::string file,
                     int line)
    {
        if (static_cast<int>(level) < static_cast<int>(this->level()))
            return;
        LogEvent ev;
        ev.timestamp = std::chrono::system_clock::now();
        ev.level = level;
        ev.logger_name = std::move(logger_name);
        ev.message = std::move(message);
        ev.file = std::move(file);
        ev.line = line;
        ev.thread_id = current_thread_id();
        const bool json = json_format();
        const std::string formatted = format_event(ev, json);

        std::vector<std::shared_ptr<LogSink>> sinks_snapshot;
        {
            std::lock_guard<std::mutex> lock(sinks_mutex_);
            sinks_snapshot = sinks_;
        }
        for (auto& s : sinks_snapshot) {
            s->write(ev, formatted);
        }
    }

    void Logger::trace(std::string logger_name, std::string message, std::string file, int line)
    {
        log(LogLevel::Trace, std::move(logger_name), std::move(message), std::move(file), line);
    }
    void Logger::debug(std::string logger_name, std::string message, std::string file, int line)
    {
        log(LogLevel::Debug, std::move(logger_name), std::move(message), std::move(file), line);
    }
    void Logger::info(std::string logger_name, std::string message, std::string file, int line)
    {
        log(LogLevel::Info, std::move(logger_name), std::move(message), std::move(file), line);
    }
    void Logger::warn(std::string logger_name, std::string message, std::string file, int line)
    {
        log(LogLevel::Warn, std::move(logger_name), std::move(message), std::move(file), line);
    }
    void Logger::error(std::string logger_name, std::string message, std::string file, int line)
    {
        log(LogLevel::Error, std::move(logger_name), std::move(message), std::move(file), line);
    }

    void Logger::flush()
    {
        std::vector<std::shared_ptr<LogSink>> sinks_snapshot;
        {
            std::lock_guard<std::mutex> lock(sinks_mutex_);
            sinks_snapshot = sinks_;
        }
        for (auto& s : sinks_snapshot) {
            s->flush();
        }
    }

} // namespace scratchbird::telemetry

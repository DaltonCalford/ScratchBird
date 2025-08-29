// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace scratchbird::telemetry
{

    enum class LogLevel : int {
        Trace = 0,
        Debug = 1,
        Info = 2,
        Warn = 3,
        Error = 4,
    };

    struct LogEvent {
        std::chrono::system_clock::time_point timestamp;
        LogLevel level;
        std::string logger_name;
        std::string message;
        std::string file;
        int line = 0;
        uint64_t thread_id = 0;
    };

    class LogSink
    {
      public:
        virtual ~LogSink() = default;
        virtual void write(const LogEvent& event, std::string_view formatted) = 0;
        virtual void flush() {}
    };

    class StdoutSink : public LogSink
    {
      public:
        void write(const LogEvent& event, std::string_view formatted) override;
        void flush() override;
    };

    class FileSink : public LogSink
    {
      public:
        // rotation_count: how many rotated files to keep (e.g., 3 keeps .1, .2, .3)
        FileSink(std::string path, std::size_t max_bytes, int rotation_count);
        ~FileSink() override;

        void write(const LogEvent& event, std::string_view formatted) override;
        void flush() override;

        const std::string& path() const
        {
            return file_path_;
        }

      private:
        void rotate_if_needed(std::size_t incoming_bytes);
        void rotate_files();

        std::string file_path_;
        std::size_t max_bytes_;
        int rotation_count_;
        std::unique_ptr<std::ostream> stream_;
        std::size_t current_size_ = 0;
        std::mutex stream_mutex_;
    };

    // Syslog sink is best-effort; if syslog is unavailable, it no-ops.
    class SyslogSink : public LogSink
    {
      public:
        SyslogSink(std::string ident);
        ~SyslogSink() override;
        void write(const LogEvent& event, std::string_view formatted) override;

      private:
        std::string ident_;
        bool available_ = false;
    };

    struct LoggerConfig {
        LogLevel level = LogLevel::Info;
        bool json_format = false;
    };

    class Logger
    {
      public:
        static Logger& instance();

        void set_level(LogLevel level);
        LogLevel level() const;

        void set_json_format(bool enabled);
        bool json_format() const;

        // Replace all sinks atomically
        void set_sinks(std::vector<std::shared_ptr<LogSink>> sinks);
        // Add a sink in addition to existing sinks
        void add_sink(std::shared_ptr<LogSink> sink);
        void clear_sinks();

        void log(LogLevel level, std::string logger_name, std::string message,
                 std::string file = {}, int line = 0);

        // Convenience helpers
        void trace(std::string logger_name, std::string message, std::string file = {},
                   int line = 0);
        void debug(std::string logger_name, std::string message, std::string file = {},
                   int line = 0);
        void info(std::string logger_name, std::string message, std::string file = {},
                  int line = 0);
        void warn(std::string logger_name, std::string message, std::string file = {},
                  int line = 0);
        void error(std::string logger_name, std::string message, std::string file = {},
                   int line = 0);

        // Flush all sinks
        void flush();

      private:
        Logger();
        static std::string format_event(const LogEvent& event, bool json);
        static uint64_t current_thread_id();

        std::atomic<int> level_int_;
        std::atomic<bool> json_format_;
        mutable std::mutex sinks_mutex_;
        std::vector<std::shared_ptr<LogSink>> sinks_;
    };

// Macros to capture file/line without overhead when disabled by level check
#define SB_LOG_AT(level, name, msg)                                                                \
    do {                                                                                           \
        auto& __logger = ::scratchbird::telemetry::Logger::instance();                             \
        if (static_cast<int>(level) >= static_cast<int>(__logger.level())) {                       \
            __logger.log((level), (name), (msg), __FILE__, __LINE__);                              \
        }                                                                                          \
    } while (0)

#define SB_LOG_TRACE(name, msg) SB_LOG_AT(::scratchbird::telemetry::LogLevel::Trace, (name), (msg))
#define SB_LOG_DEBUG(name, msg) SB_LOG_AT(::scratchbird::telemetry::LogLevel::Debug, (name), (msg))
#define SB_LOG_INFO(name, msg) SB_LOG_AT(::scratchbird::telemetry::LogLevel::Info, (name), (msg))
#define SB_LOG_WARN(name, msg) SB_LOG_AT(::scratchbird::telemetry::LogLevel::Warn, (name), (msg))
#define SB_LOG_ERROR(name, msg) SB_LOG_AT(::scratchbird::telemetry::LogLevel::Error, (name), (msg))

} // namespace scratchbird::telemetry

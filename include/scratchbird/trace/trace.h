// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace scratchbird::trace
{

    struct TraceEvent {
        std::chrono::steady_clock::time_point ts;
        std::string category;
        std::string name;
        uint64_t tid;
        uint64_t span_id;
        uint64_t parent_span_id;
    };

    struct TraceProfileConfig {
        std::string name;
        double sample_ratio = 0.0;           // 0..1
        std::size_t buffer_capacity = 16384; // number of events
    };

    class TraceBuffer
    {
      public:
        explicit TraceBuffer(std::size_t capacity);
        void clear();
        void push(TraceEvent ev);
        std::vector<TraceEvent> snapshot() const;

      private:
        std::vector<TraceEvent> ring_;
        mutable std::mutex mu_;
        std::size_t head_ = 0;
        bool full_ = false;
    };

    class TraceController
    {
      public:
        static TraceController& instance();

        void start_profile(const TraceProfileConfig& cfg);
        void stop_profile();
        bool is_running() const;
        TraceProfileConfig current_config() const;

        bool should_sample() const; // fast path
        void emit(std::string category, std::string name, uint64_t span_id,
                  uint64_t parent_span_id);
        std::vector<TraceEvent> collect();
        uint64_t allocate_span_id();

      private:
        TraceController();
        static uint64_t current_thread_id();

        mutable std::mutex mu_;
        std::optional<TraceProfileConfig> active_;
        std::unique_ptr<TraceBuffer> buffer_;
        std::atomic<uint64_t> span_id_counter_;
    };

    // RAII span helper
    class TraceSpan
    {
      public:
        explicit TraceSpan(std::string category, std::string name, uint64_t parent_span_id = 0);
        ~TraceSpan();

        uint64_t id() const
        {
            return span_id_;
        }

      private:
        bool sampled_ = false;
        uint64_t span_id_ = 0;
        uint64_t parent_id_ = 0;
        std::string category_;
        std::string name_;
    };

} // namespace scratchbird::trace

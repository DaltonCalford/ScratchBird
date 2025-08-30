// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace scratchbird::engine
{

    // Forward declarations
    class BufferPool;
    class BufferDescriptor;
    struct BufferTag;

    /// Configuration for background writer behavior
    struct BackgroundWriterConfig {
        /// Time between background write cycles (milliseconds)
        std::chrono::milliseconds write_interval{100};

        /// Maximum number of buffers to write per cycle
        std::uint32_t max_buffers_per_cycle{100};

        /// Minimum number of dirty buffers to trigger writing
        std::uint32_t dirty_buffer_threshold{10};

        /// Maximum I/O rate in bytes per second (0 = unlimited)
        std::uint64_t max_io_rate{0};

        /// Enable/disable background writer
        bool enabled{true};

        /// Use O_SYNC for write operations
        bool use_sync_writes{false};
    };

    /// Statistics snapshot for background writer performance monitoring
    struct BackgroundWriterStats {
        /// Total number of background writer cycles
        std::uint64_t write_cycles{0};

        /// Total number of buffers written
        std::uint64_t buffers_written{0};

        /// Total bytes written
        std::uint64_t bytes_written{0};

        /// Number of write batches processed
        std::uint64_t batches_processed{0};

        /// Total time spent writing (microseconds)
        std::uint64_t total_write_time_us{0};

        /// Number of I/O operations performed
        std::uint64_t io_operations{0};

        /// Last write cycle timestamp
        std::uint64_t last_write_timestamp{0};

        /// Average write latency (microseconds)
        std::uint64_t avg_write_latency_us{0};

        /// Peak dirty buffer count observed
        std::uint32_t peak_dirty_buffers{0};
    };

    /// Internal statistics with atomic members for thread-safe updates
    struct BackgroundWriterStatsAtomic {
        /// Total number of background writer cycles
        std::atomic<std::uint64_t> write_cycles{0};

        /// Total number of buffers written
        std::atomic<std::uint64_t> buffers_written{0};

        /// Total bytes written
        std::atomic<std::uint64_t> bytes_written{0};

        /// Number of write batches processed
        std::atomic<std::uint64_t> batches_processed{0};

        /// Total time spent writing (microseconds)
        std::atomic<std::uint64_t> total_write_time_us{0};

        /// Number of I/O operations performed
        std::atomic<std::uint64_t> io_operations{0};

        /// Last write cycle timestamp
        std::atomic<std::uint64_t> last_write_timestamp{0};

        /// Average write latency (microseconds)
        std::atomic<std::uint64_t> avg_write_latency_us{0};

        /// Peak dirty buffer count observed
        std::atomic<std::uint32_t> peak_dirty_buffers{0};
    };

    /// Represents a batch of buffers to be written together
    struct WriteBatch {
        std::vector<BufferDescriptor*> buffers;
        std::uint32_t total_size_bytes{0};
        std::chrono::steady_clock::time_point created_at;

        WriteBatch() : created_at(std::chrono::steady_clock::now()) {}

        void add_buffer(BufferDescriptor* buffer, std::uint32_t buffer_size)
        {
            buffers.push_back(buffer);
            total_size_bytes += buffer_size;
        }

        bool empty() const
        {
            return buffers.empty();
        }
        std::size_t size() const
        {
            return buffers.size();
        }
        void clear()
        {
            buffers.clear();
            total_size_bytes = 0;
            created_at = std::chrono::steady_clock::now();
        }
    };

    /// Background writer manages proactive writing of dirty buffers to reduce foreground I/O
    /// blocking
    class BackgroundWriter
    {
      public:
        /// Constructor takes buffer pool reference and optional configuration
        explicit BackgroundWriter(BufferPool* buffer_pool,
                                  const BackgroundWriterConfig& config = BackgroundWriterConfig{});

        /// Destructor ensures clean shutdown
        ~BackgroundWriter();

        // Non-copyable
        BackgroundWriter(const BackgroundWriter&) = delete;
        BackgroundWriter& operator=(const BackgroundWriter&) = delete;

        /// Start the background writer thread
        void start();

        /// Stop the background writer thread (graceful shutdown)
        void stop();

        /// Check if background writer is running
        bool is_running() const;

        /// Update configuration at runtime
        void update_config(const BackgroundWriterConfig& config);

        /// Get current configuration
        BackgroundWriterConfig get_config() const;

        /// Get performance statistics
        BackgroundWriterStats get_stats() const;

        /// Reset performance statistics
        void reset_stats();

        /// Force an immediate write cycle (for testing/emergency)
        void force_write_cycle();

        /// Get average write latency in microseconds
        double get_avg_write_latency_us() const;

        /// Get write throughput in bytes per second
        double get_write_throughput_bps() const;

      private:
        /// Main background writer loop
        void writer_thread_main();

        /// Perform one write cycle
        void perform_write_cycle();

        /// Collect dirty buffers for writing
        std::uint32_t collect_dirty_buffers(WriteBatch& batch);

        /// Write a batch of buffers to disk
        void write_batch(const WriteBatch& batch);

        /// Sort buffers in batch for optimal I/O ordering
        void optimize_batch_order(WriteBatch& batch);

        /// Check if I/O rate limiting should be applied
        bool should_throttle_io() const;

        /// Apply I/O throttling delay if necessary
        void apply_io_throttling(std::uint64_t bytes_written);

        /// Update performance statistics
        void update_stats(const WriteBatch& batch, std::chrono::microseconds write_time);

        /// Buffer pool reference
        BufferPool* buffer_pool_;

        /// Configuration (protected by config_mutex_)
        BackgroundWriterConfig config_;
        mutable std::mutex config_mutex_;

        /// Performance statistics
        BackgroundWriterStatsAtomic stats_;

        /// Background writer thread
        std::unique_ptr<std::thread> writer_thread_;

        /// Thread synchronization
        mutable std::mutex thread_mutex_;
        std::condition_variable thread_cv_;

        /// Shutdown control
        std::atomic<bool> shutdown_requested_{false};
        std::atomic<bool> running_{false};

        /// I/O rate limiting state
        mutable std::mutex throttle_mutex_;
        std::chrono::steady_clock::time_point last_throttle_check_;
        std::uint64_t bytes_written_since_throttle_check_{0};
    };

} // namespace scratchbird::engine

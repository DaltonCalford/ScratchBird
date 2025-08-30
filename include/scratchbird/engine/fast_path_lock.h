// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "scratchbird/engine/ods.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

namespace scratchbird::engine
{

    /// Lock types for fast-path locking system
    enum class FastPathLockType : std::uint8_t {
        NONE = 0,
        ACCESS_SHARE_LOCK = 1,      ///< Shared read lock (SELECT)
        ROW_SHARE_LOCK = 2,         ///< Row-level shared lock (SELECT FOR UPDATE)
        ROW_EXCLUSIVE_LOCK = 3,     ///< Row-level exclusive lock (UPDATE, DELETE, INSERT)
        SHARE_UPDATE_EXCLUSIVE = 4, ///< Shared update exclusive lock
        SHARE_LOCK = 5,             ///< Table-level shared lock
        SHARE_ROW_EXCLUSIVE = 6,    ///< Share row exclusive lock
        EXCLUSIVE_LOCK = 7,         ///< Table-level exclusive lock
        ACCESS_EXCLUSIVE_LOCK = 8   ///< Full exclusive lock (DDL operations)
    };

    /// Lock mode conflicts matrix (compatibility) - PostgreSQL compatible
    constexpr bool LOCK_CONFLICT_TABLE[9][9] = {
        // NONE  AS   RS   RE   SUE  S    SRE  EX   AE
        {false, false, false, false, false, false, false, false, false}, // NONE
        {false, false, false, false, false, false, false, true, true},   // ACCESS_SHARE
        {false, false, false, false, false, false, true, true, true},    // ROW_SHARE
        {false, false, false, false, true, true, true, true, true},      // ROW_EXCLUSIVE
        {false, false, false, true, true, true, true, true, true},       // SHARE_UPDATE_EXCLUSIVE
        {false, false, false, true, true, false, true, true, true},      // SHARE
        {false, false, true, true, true, true, true, true, true},        // SHARE_ROW_EXCLUSIVE
        {false, true, true, true, true, true, true, true, true},         // EXCLUSIVE
        {false, true, true, true, true, true, true, true, true}          // ACCESS_EXCLUSIVE
    };

    /// Check if two lock types conflict
    constexpr bool lock_conflicts(FastPathLockType held, FastPathLockType requested)
    {
        return LOCK_CONFLICT_TABLE[static_cast<int>(held)][static_cast<int>(requested)];
    }

    /// Fast-path lock entry for per-backend storage
    struct FastPathLockEntry {
        /// Resource identifier (table OID, relation OID, etc.)
        std::atomic<std::uint64_t> resource_id{0};

        /// Lock type currently held
        std::atomic<FastPathLockType> lock_type{FastPathLockType::NONE};

        /// Transaction ID holding the lock
        std::atomic<std::uint64_t> holder_xid{0};

        /// Reference count for lock reacquisition
        std::atomic<std::uint32_t> ref_count{0};

        /// Timestamp when lock was acquired
        std::atomic<std::uint64_t> acquire_timestamp{0};

        /// Default constructor
        FastPathLockEntry() = default;

        /// Initialize entry
        void initialize(std::uint64_t resource, FastPathLockType type, std::uint64_t xid)
        {
            resource_id.store(resource, std::memory_order_relaxed);
            lock_type.store(type, std::memory_order_relaxed);
            holder_xid.store(xid, std::memory_order_relaxed);
            ref_count.store(1, std::memory_order_relaxed);
            auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
            acquire_timestamp.store(now, std::memory_order_relaxed);
        }

        /// Clear entry
        void clear()
        {
            resource_id.store(0, std::memory_order_relaxed);
            lock_type.store(FastPathLockType::NONE, std::memory_order_relaxed);
            holder_xid.store(0, std::memory_order_relaxed);
            ref_count.store(0, std::memory_order_relaxed);
            acquire_timestamp.store(0, std::memory_order_relaxed);
        }

        /// Check if entry is free
        bool is_free() const
        {
            return lock_type.load(std::memory_order_relaxed) == FastPathLockType::NONE;
        }

        /// Check if entry matches resource and transaction
        bool matches(std::uint64_t resource, std::uint64_t xid) const
        {
            return resource_id.load(std::memory_order_relaxed) == resource &&
                   holder_xid.load(std::memory_order_relaxed) == xid && !is_free();
        }
    };

    /// Fast-path lock statistics
    struct FastPathLockStats {
        /// Fast-path lock acquisition attempts
        std::atomic<std::uint64_t> fast_path_attempts{0};

        /// Successful fast-path lock acquisitions
        std::atomic<std::uint64_t> fast_path_successes{0};

        /// Fast-path lock releases
        std::atomic<std::uint64_t> fast_path_releases{0};

        /// Fallbacks to regular lock manager
        std::atomic<std::uint64_t> fallback_count{0};

        /// Lock conflicts detected in fast-path
        std::atomic<std::uint64_t> conflict_count{0};

        /// Array overflow events (all slots used)
        std::atomic<std::uint64_t> overflow_count{0};

        /// Total time spent in fast-path operations (microseconds)
        std::atomic<std::uint64_t> total_time_us{0};

        /// Average lock acquisition time (microseconds)
        std::atomic<std::uint64_t> avg_acquisition_time_us{0};

        /// Lock type distribution counters
        std::array<std::atomic<std::uint64_t>, 9> lock_type_counts{};

        /// Default constructor
        FastPathLockStats() = default;

        /// Copy constructor
        FastPathLockStats(const FastPathLockStats& other)
        {
            copy_from(other);
        }

        /// Copy assignment
        FastPathLockStats& operator=(const FastPathLockStats& other)
        {
            if (this != &other) {
                copy_from(other);
            }
            return *this;
        }

        /// Calculate fast-path success ratio
        double get_success_ratio() const
        {
            auto attempts = fast_path_attempts.load();
            if (attempts == 0)
                return 0.0;
            return static_cast<double>(fast_path_successes.load()) / attempts;
        }

        /// Update acquisition time statistics
        void update_timing(std::uint64_t time_us)
        {
            total_time_us.fetch_add(time_us);
            auto successes = fast_path_successes.load();
            if (successes > 0) {
                avg_acquisition_time_us.store(total_time_us.load() / successes);
            }
        }

        /// Reset statistics
        void reset()
        {
            fast_path_attempts.store(0);
            fast_path_successes.store(0);
            fast_path_releases.store(0);
            fallback_count.store(0);
            conflict_count.store(0);
            overflow_count.store(0);
            total_time_us.store(0);
            avg_acquisition_time_us.store(0);
            for (auto& counter : lock_type_counts) {
                counter.store(0);
            }
        }

      private:
        /// Copy atomic values from another instance
        void copy_from(const FastPathLockStats& other)
        {
            fast_path_attempts.store(other.fast_path_attempts.load());
            fast_path_successes.store(other.fast_path_successes.load());
            fast_path_releases.store(other.fast_path_releases.load());
            fallback_count.store(other.fallback_count.load());
            conflict_count.store(other.conflict_count.load());
            overflow_count.store(other.overflow_count.load());
            total_time_us.store(other.total_time_us.load());
            avg_acquisition_time_us.store(other.avg_acquisition_time_us.load());
            for (size_t i = 0; i < lock_type_counts.size(); ++i) {
                lock_type_counts[i].store(other.lock_type_counts[i].load());
            }
        }
    };

    /// Fast-path lock configuration
    struct FastPathLockConfig {
        /// Number of fast-path lock slots per backend
        std::uint32_t max_fast_path_locks{16};

        /// Enable/disable fast-path locking
        bool enabled{true};

        /// Enable/disable statistics collection
        bool collect_statistics{true};

        /// Maximum lock hold time before forcing cleanup (seconds)
        std::uint32_t max_lock_hold_time_seconds{300}; // 5 minutes

        /// Lock cleanup interval (seconds)
        std::uint32_t cleanup_interval_seconds{60}; // 1 minute

        /// Enable/disable automatic cleanup of expired locks
        bool enable_automatic_cleanup{true};

        /// Validation
        bool is_valid() const
        {
            return max_fast_path_locks > 0 && max_fast_path_locks <= 1024 &&
                   max_lock_hold_time_seconds > 0 && cleanup_interval_seconds > 0;
        }
    };

    /// Per-backend fast-path lock array
    class FastPathLockArray
    {
      public:
        /// Constructor
        explicit FastPathLockArray(std::uint32_t max_locks = 16);

        /// Destructor
        ~FastPathLockArray() = default;

        /// Non-copyable, moveable
        FastPathLockArray(const FastPathLockArray&) = delete;
        FastPathLockArray& operator=(const FastPathLockArray&) = delete;
        FastPathLockArray(FastPathLockArray&&) = default;
        FastPathLockArray& operator=(FastPathLockArray&&) = default;

        /// Try to acquire a fast-path lock
        bool try_acquire_lock(std::uint64_t resource_id, FastPathLockType lock_type,
                              std::uint64_t xid);

        /// Release a fast-path lock
        bool release_lock(std::uint64_t resource_id, std::uint64_t xid);

        /// Check if a lock can be acquired (conflict detection)
        bool can_acquire_lock(std::uint64_t resource_id, FastPathLockType lock_type,
                              std::uint64_t xid) const;

        /// Get lock information for a resource
        std::pair<FastPathLockType, std::uint64_t> get_lock_info(std::uint64_t resource_id) const;

        /// Clear all locks held by a transaction
        std::uint32_t clear_transaction_locks(std::uint64_t xid);

        /// Get count of locks held by transaction
        std::uint32_t get_transaction_lock_count(std::uint64_t xid) const;

        /// Clean up expired locks
        std::uint32_t cleanup_expired_locks(std::uint64_t max_age_seconds);

        /// Get array utilization (0.0 to 1.0)
        double get_utilization() const;

        /// Get number of used slots
        std::uint32_t get_used_slots() const;

        /// Check if array is full
        bool is_full() const;

      private:
        /// Maximum number of fast-path locks
        std::uint32_t max_locks_;

        /// Fast-path lock entries
        std::unique_ptr<FastPathLockEntry[]> entries_;

        /// Mutex for array operations
        mutable std::mutex array_mutex_;

        /// Find free slot in the array
        std::int32_t find_free_slot() const;

        /// Find slot for specific resource and transaction
        std::int32_t find_slot(std::uint64_t resource_id, std::uint64_t xid) const;

        /// Check for conflicts with existing locks
        bool has_conflicts(std::uint64_t resource_id, FastPathLockType lock_type,
                           std::uint64_t xid) const;
    };

    /// Fast-Path Lock Manager
    class FastPathLockManager
    {
      public:
        /// Constructor
        explicit FastPathLockManager(const FastPathLockConfig& config = FastPathLockConfig{});

        /// Destructor
        ~FastPathLockManager();

        /// Non-copyable, non-moveable
        FastPathLockManager(const FastPathLockManager&) = delete;
        FastPathLockManager& operator=(const FastPathLockManager&) = delete;
        FastPathLockManager(FastPathLockManager&&) = delete;
        FastPathLockManager& operator=(FastPathLockManager&&) = delete;

        /// Initialize the fast-path lock manager
        bool initialize();

        /// Shutdown the fast-path lock manager
        void shutdown();

        /// Lock operations

        /// Try to acquire a fast-path lock
        bool try_fast_path_lock(std::uint64_t resource_id, FastPathLockType lock_type,
                                std::uint64_t xid);

        /// Release a fast-path lock
        bool release_fast_path_lock(std::uint64_t resource_id, std::uint64_t xid);

        /// Check if fast-path can handle this lock request
        bool is_fast_path_eligible(std::uint64_t resource_id, FastPathLockType lock_type,
                                   std::uint64_t xid) const;

        /// Clear all locks held by a transaction
        std::uint32_t clear_transaction_locks(std::uint64_t xid);

        /// Backend management

        /// Register a backend thread for fast-path locking
        bool register_backend(std::thread::id thread_id);

        /// Unregister a backend thread
        void unregister_backend(std::thread::id thread_id);

        /// Get or create lock array for current thread
        FastPathLockArray* get_current_thread_array();

        /// Get lock array for current thread (const version)
        const FastPathLockArray* get_current_thread_array_const() const;

        /// Configuration and statistics

        /// Update configuration
        bool update_config(const FastPathLockConfig& config);

        /// Get current configuration
        FastPathLockConfig get_config() const;

        /// Get aggregated statistics
        FastPathLockStats get_statistics() const;

        /// Reset statistics
        void reset_statistics();

        /// Monitoring and diagnostics

        /// Get per-thread lock information
        std::unordered_map<std::thread::id, std::uint32_t> get_per_thread_lock_counts() const;

        /// Get total number of registered backends
        std::uint32_t get_backend_count() const;

        /// Get system-wide lock utilization
        double get_system_utilization() const;

        /// Generate performance report
        std::string generate_performance_report() const;

        /// Performance monitoring

        /// Enable/disable performance monitoring
        void set_monitoring_enabled(bool enabled);

        /// Get performance metrics for specific lock type
        std::uint64_t get_lock_type_count(FastPathLockType lock_type) const;

      private:
        /// Configuration
        FastPathLockConfig config_;
        mutable std::mutex config_mutex_;

        /// Per-thread lock arrays
        std::unordered_map<std::thread::id, std::unique_ptr<FastPathLockArray>> thread_arrays_;
        mutable std::shared_mutex arrays_mutex_;

        /// Global statistics
        FastPathLockStats global_stats_;
        mutable std::mutex stats_mutex_;

        /// Background cleanup thread
        std::unique_ptr<std::thread> cleanup_thread_;
        std::atomic<bool> shutdown_requested_{false};

        /// Private methods

        /// Background cleanup thread main function
        void cleanup_thread_main();

        /// Perform cleanup of expired locks across all threads
        std::uint32_t cleanup_all_expired_locks();

        /// Update global statistics from thread-local stats
        void update_global_statistics();
    };

    /// Helper functions for lock type conversion and utilities

    /// Convert FastPathLockType to string
    const char* lock_type_to_string(FastPathLockType lock_type);

    /// Check if lock type is suitable for fast-path
    bool is_fast_path_suitable(FastPathLockType lock_type);

    /// Get lock type priority (for conflict resolution)
    std::uint32_t get_lock_type_priority(FastPathLockType lock_type);

    /// Create resource ID from table OID and optional row ID
    std::uint64_t make_resource_id(std::uint32_t table_oid, std::uint32_t row_id = 0);

    /// Extract table OID from resource ID
    std::uint32_t get_table_oid_from_resource_id(std::uint64_t resource_id);

    /// Extract row ID from resource ID
    std::uint32_t get_row_id_from_resource_id(std::uint64_t resource_id);

} // namespace scratchbird::engine

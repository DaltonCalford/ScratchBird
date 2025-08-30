// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "scratchbird/engine/fast_path_lock.h"
#include "scratchbird/engine/ods.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scratchbird::engine
{

    /// Lock request for partitioned lock manager
    struct LockRequest {
        std::uint64_t resource_id{0};                       ///< Resource being locked
        FastPathLockType lock_type{FastPathLockType::NONE}; ///< Type of lock requested
        std::uint64_t holder_xid{0};                        ///< Transaction holding the lock
        std::chrono::steady_clock::time_point acquire_time; ///< When lock was acquired
        bool granted{false};                                ///< Whether lock has been granted

        LockRequest() = default;

        LockRequest(std::uint64_t resource, FastPathLockType type, std::uint64_t xid)
            : resource_id(resource), lock_type(type), holder_xid(xid),
              acquire_time(std::chrono::steady_clock::now()), granted(false)
        {
        }
    };

    /// Deadlock detection result
    struct DeadlockInfo {
        bool detected{false};             ///< Whether deadlock was detected
        std::vector<std::uint64_t> cycle; ///< Transaction IDs in deadlock cycle
        std::uint64_t victim_xid{0};      ///< Chosen victim transaction
        std::string description;          ///< Human-readable description
    };

    /// Statistics for a single lock partition
    struct PartitionStatistics {
        /// Lock operations
        std::atomic<std::uint64_t> lock_requests{0};  ///< Total lock requests
        std::atomic<std::uint64_t> lock_grants{0};    ///< Successful lock grants
        std::atomic<std::uint64_t> lock_conflicts{0}; ///< Lock conflicts encountered
        std::atomic<std::uint64_t> lock_releases{0};  ///< Lock releases
        std::atomic<std::uint64_t> lock_waits{0};     ///< Locks that had to wait

        /// Deadlock detection
        std::atomic<std::uint64_t> deadlock_checks{0};    ///< Deadlock detection runs
        std::atomic<std::uint64_t> deadlocks_detected{0}; ///< Deadlocks found
        std::atomic<std::uint64_t> deadlock_victims{0};   ///< Transactions aborted due to deadlock

        /// Timing statistics
        std::atomic<std::uint64_t> total_wait_time_us{0}; ///< Total wait time in microseconds
        std::atomic<std::uint64_t> avg_wait_time_us{0};   ///< Average wait time per lock

        /// Lock table utilization
        std::atomic<std::uint32_t> active_locks{0};    ///< Currently held locks
        std::atomic<std::uint32_t> max_locks_held{0};  ///< Peak lock count
        std::atomic<std::uint32_t> lock_table_size{0}; ///< Current lock table size

        /// Default constructor
        PartitionStatistics() = default;

        /// Copy constructor
        PartitionStatistics(const PartitionStatistics& other)
        {
            copy_from(other);
        }

        /// Copy assignment
        PartitionStatistics& operator=(const PartitionStatistics& other)
        {
            if (this != &other) {
                copy_from(other);
            }
            return *this;
        }

        /// Reset all statistics
        void reset()
        {
            lock_requests.store(0);
            lock_grants.store(0);
            lock_conflicts.store(0);
            lock_releases.store(0);
            lock_waits.store(0);
            deadlock_checks.store(0);
            deadlocks_detected.store(0);
            deadlock_victims.store(0);
            total_wait_time_us.store(0);
            avg_wait_time_us.store(0);
            active_locks.store(0);
            max_locks_held.store(0);
            lock_table_size.store(0);
        }

      private:
        void copy_from(const PartitionStatistics& other)
        {
            lock_requests.store(other.lock_requests.load());
            lock_grants.store(other.lock_grants.load());
            lock_conflicts.store(other.lock_conflicts.load());
            lock_releases.store(other.lock_releases.load());
            lock_waits.store(other.lock_waits.load());
            deadlock_checks.store(other.deadlock_checks.load());
            deadlocks_detected.store(other.deadlocks_detected.load());
            deadlock_victims.store(other.deadlock_victims.load());
            total_wait_time_us.store(other.total_wait_time_us.load());
            avg_wait_time_us.store(other.avg_wait_time_us.load());
            active_locks.store(other.active_locks.load());
            max_locks_held.store(other.max_locks_held.load());
            lock_table_size.store(other.lock_table_size.load());
        }
    };

    /// Configuration for partitioned lock manager
    struct PartitionedLockManagerConfig {
        /// Number of lock partitions (must be power of 2 for efficient hashing)
        std::uint32_t partition_count{16};

        /// Maximum locks per partition before resizing
        std::uint32_t max_locks_per_partition{10000};

        /// Enable/disable deadlock detection
        bool deadlock_detection_enabled{true};

        /// Deadlock detection interval (milliseconds)
        std::uint32_t deadlock_check_interval_ms{1000};

        /// Maximum wait time before timeout (milliseconds)
        std::uint32_t max_wait_time_ms{30000}; // 30 seconds

        /// Enable/disable statistics collection
        bool collect_statistics{true};

        /// Enable/disable cross-partition deadlock detection
        bool cross_partition_deadlock_detection{true};

        /// Lock hash table initial size per partition
        std::uint32_t initial_table_size{1024};

        /// Load factor threshold for hash table resizing
        double load_factor_threshold{0.75};

        /// Validate configuration parameters
        bool is_valid() const
        {
            return partition_count > 0 &&
                   (partition_count & (partition_count - 1)) == 0 && // Power of 2
                   max_locks_per_partition > 0 && deadlock_check_interval_ms > 0 &&
                   max_wait_time_ms > 0 && initial_table_size > 0 && load_factor_threshold > 0.0 &&
                   load_factor_threshold < 1.0;
        }
    };

    /// Single partition of the lock manager
    class LockPartition
    {
      public:
        /// Constructor
        explicit LockPartition(std::uint32_t partition_id,
                               const PartitionedLockManagerConfig& config);

        /// Destructor
        ~LockPartition() = default;

        /// Non-copyable, moveable
        LockPartition(const LockPartition&) = delete;
        LockPartition& operator=(const LockPartition&) = delete;
        LockPartition(LockPartition&&) = default;
        LockPartition& operator=(LockPartition&&) = default;

        /// Lock operations

        /// Try to acquire a lock (non-blocking)
        bool try_lock(std::uint64_t resource_id, FastPathLockType lock_type, std::uint64_t xid);

        /// Acquire a lock (blocking with timeout)
        bool acquire_lock(std::uint64_t resource_id, FastPathLockType lock_type, std::uint64_t xid,
                          std::chrono::milliseconds timeout = std::chrono::milliseconds{30000});

        /// Release a lock
        bool release_lock(std::uint64_t resource_id, std::uint64_t xid);

        /// Release all locks held by a transaction
        std::uint32_t release_transaction_locks(std::uint64_t xid);

        /// Check if a lock request would conflict
        bool would_conflict(std::uint64_t resource_id, FastPathLockType lock_type,
                            std::uint64_t xid) const;

        /// Get lock information for a resource
        std::pair<FastPathLockType, std::uint64_t> get_lock_info(std::uint64_t resource_id) const;

        /// Deadlock detection

        /// Check for deadlocks involving transactions in this partition
        DeadlockInfo check_for_deadlocks();

        /// Add wait-for edge (used for deadlock detection)
        void add_wait_for_edge(std::uint64_t waiter_xid, std::uint64_t holder_xid);

        /// Remove wait-for edge
        void remove_wait_for_edge(std::uint64_t waiter_xid, std::uint64_t holder_xid);

        /// Statistics and monitoring

        /// Get partition statistics
        PartitionStatistics get_statistics() const;

        /// Reset partition statistics
        void reset_statistics();

        /// Get partition utilization (0.0 to 1.0)
        double get_utilization() const;

        /// Get active lock count
        std::uint32_t get_active_lock_count() const;

        /// Get wait-for graph size
        std::uint32_t get_wait_for_graph_size() const;

        /// Maintenance operations

        /// Cleanup expired locks and wait-for edges
        std::uint32_t cleanup_expired_entries();

        /// Resize lock table if needed
        bool resize_if_needed();

      private:
        /// Partition ID
        std::uint32_t partition_id_;

        /// Configuration
        PartitionedLockManagerConfig config_;

        /// Lock table: resource_id -> list of lock requests
        std::unordered_map<std::uint64_t, std::vector<LockRequest>> lock_table_;
        mutable std::shared_mutex lock_table_mutex_;

        /// Wait-for graph: waiter_xid -> set of holder_xids
        std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>> wait_for_graph_;
        mutable std::mutex wait_for_mutex_;

        /// Statistics
        mutable PartitionStatistics stats_;

        /// Private helper methods

        /// Check if two lock types conflict
        bool locks_conflict(FastPathLockType held_type, FastPathLockType requested_type) const;

        /// Find lock in lock table
        std::vector<LockRequest>::iterator find_lock(std::vector<LockRequest>& requests,
                                                     std::uint64_t xid);

        /// Detect deadlock cycle starting from a transaction
        std::vector<std::uint64_t> detect_cycle(std::uint64_t start_xid,
                                                std::unordered_set<std::uint64_t>& visited,
                                                std::unordered_set<std::uint64_t>& rec_stack) const;

        /// Choose victim from deadlock cycle
        std::uint64_t choose_deadlock_victim(const std::vector<std::uint64_t>& cycle) const;

        /// Update timing statistics
        void update_wait_time_stats(std::uint64_t wait_time_us);
    };

    /// Partitioned Lock Manager - distributes locks across multiple partitions
    class PartitionedLockManager
    {
      public:
        /// Constructor
        explicit PartitionedLockManager(
            const PartitionedLockManagerConfig& config = PartitionedLockManagerConfig{});

        /// Destructor
        ~PartitionedLockManager();

        /// Non-copyable, non-moveable
        PartitionedLockManager(const PartitionedLockManager&) = delete;
        PartitionedLockManager& operator=(const PartitionedLockManager&) = delete;
        PartitionedLockManager(PartitionedLockManager&&) = delete;
        PartitionedLockManager& operator=(PartitionedLockManager&&) = delete;

        /// Initialization

        /// Initialize the partitioned lock manager
        bool initialize();

        /// Shutdown the partitioned lock manager
        void shutdown();

        /// Lock operations

        /// Try to acquire a lock (non-blocking)
        bool try_lock(std::uint64_t resource_id, FastPathLockType lock_type, std::uint64_t xid);

        /// Acquire a lock (blocking with timeout)
        bool acquire_lock(std::uint64_t resource_id, FastPathLockType lock_type, std::uint64_t xid,
                          std::chrono::milliseconds timeout = std::chrono::milliseconds{30000});

        /// Release a lock
        bool release_lock(std::uint64_t resource_id, std::uint64_t xid);

        /// Release all locks held by a transaction
        std::uint32_t release_transaction_locks(std::uint64_t xid);

        /// Check if a lock request would conflict
        bool would_conflict(std::uint64_t resource_id, FastPathLockType lock_type,
                            std::uint64_t xid) const;

        /// Get lock information for a resource
        std::pair<FastPathLockType, std::uint64_t> get_lock_info(std::uint64_t resource_id) const;

        /// Cross-partition deadlock detection

        /// Check for deadlocks across all partitions
        std::vector<DeadlockInfo> check_for_deadlocks();

        /// Enable/disable cross-partition deadlock detection
        void set_cross_partition_deadlock_detection(bool enabled);

        /// Configuration management

        /// Update configuration (thread-safe)
        bool update_config(const PartitionedLockManagerConfig& config);

        /// Get current configuration
        PartitionedLockManagerConfig get_config() const;

        /// Statistics and monitoring

        /// Get aggregated statistics across all partitions
        PartitionStatistics get_aggregated_statistics() const;

        /// Get per-partition statistics
        std::vector<PartitionStatistics> get_per_partition_statistics() const;

        /// Reset all statistics
        void reset_statistics();

        /// Get system-wide utilization
        double get_system_utilization() const;

        /// Get total active lock count
        std::uint64_t get_total_active_lock_count() const;

        /// Generate performance report
        std::string generate_performance_report() const;

        /// Maintenance operations

        /// Perform maintenance on all partitions
        std::uint32_t perform_maintenance();

        /// Get partition for a resource ID
        std::uint32_t get_partition_id(std::uint64_t resource_id) const;

      private:
        /// Configuration
        PartitionedLockManagerConfig config_;
        mutable std::mutex config_mutex_;

        /// Lock partitions
        std::vector<std::unique_ptr<LockPartition>> partitions_;

        /// Background deadlock detection thread
        std::unique_ptr<std::thread> deadlock_detector_thread_;
        std::atomic<bool> shutdown_requested_{false};

        /// Maintenance thread
        std::unique_ptr<std::thread> maintenance_thread_;

        /// Private methods

        /// Background deadlock detection thread main function
        void deadlock_detector_main();

        /// Background maintenance thread main function
        void maintenance_thread_main();

        /// Hash function for resource ID to partition mapping
        std::uint32_t hash_to_partition(std::uint64_t resource_id) const;

        /// Cross-partition deadlock detection
        DeadlockInfo detect_cross_partition_deadlock();

        /// Collect wait-for edges from all partitions
        std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>>
        collect_global_wait_for_graph() const;
    };

    /// Helper functions for partitioned locking

    /// Calculate optimal partition count based on core count and workload
    std::uint32_t calculate_optimal_partition_count(std::uint32_t core_count = 0);

    /// Validate partitioned lock manager configuration
    bool validate_partition_config(const PartitionedLockManagerConfig& config);

    /// Create default configuration optimized for current system
    PartitionedLockManagerConfig create_default_partition_config();

} // namespace scratchbird::engine

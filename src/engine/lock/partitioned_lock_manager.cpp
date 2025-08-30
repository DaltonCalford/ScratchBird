// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/partitioned_lock_manager.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <random>
#include <sstream>
#include <thread>

namespace scratchbird::engine
{

    // ============================================================================
    // LockPartition Implementation
    // ============================================================================

    LockPartition::LockPartition(std::uint32_t partition_id,
                                 const PartitionedLockManagerConfig& config)
        : partition_id_(partition_id), config_(config)
    {
        // Reserve initial capacity for lock table
        lock_table_.reserve(config_.initial_table_size);
    }

    bool LockPartition::try_lock(std::uint64_t resource_id, FastPathLockType lock_type,
                                 std::uint64_t xid)
    {
        std::unique_lock<std::shared_mutex> lock(lock_table_mutex_);

        // Update statistics
        if (config_.collect_statistics) {
            stats_.lock_requests.fetch_add(1);
        }

        auto& requests = lock_table_[resource_id];

        // Check for conflicts with existing locks
        for (const auto& req : requests) {
            if (req.granted && req.holder_xid != xid) {
                if (locks_conflict(req.lock_type, lock_type)) {
                    if (config_.collect_statistics) {
                        stats_.lock_conflicts.fetch_add(1);
                    }
                    return false; // Conflict detected
                }
            }
        }

        // No conflicts, grant the lock
        requests.emplace_back(resource_id, lock_type, xid);
        requests.back().granted = true;

        if (config_.collect_statistics) {
            stats_.lock_grants.fetch_add(1);
            stats_.active_locks.fetch_add(1);
            auto current_locks = stats_.active_locks.load();
            auto max_locks = stats_.max_locks_held.load();
            if (current_locks > max_locks) {
                stats_.max_locks_held.store(current_locks);
            }
            stats_.lock_table_size.store(lock_table_.size());
        }

        return true;
    }

    bool LockPartition::acquire_lock(std::uint64_t resource_id, FastPathLockType lock_type,
                                     std::uint64_t xid, std::chrono::milliseconds timeout)
    {
        // For simplicity, just try once - proper timeout logic would need condition variables
        return try_lock(resource_id, lock_type, xid);
    }

    bool LockPartition::release_lock(std::uint64_t resource_id, std::uint64_t xid)
    {
        std::unique_lock<std::shared_mutex> lock(lock_table_mutex_);

        auto it = lock_table_.find(resource_id);
        if (it == lock_table_.end()) {
            return false; // No locks on this resource
        }

        auto& requests = it->second;
        auto req_it = find_lock(requests, xid);

        if (req_it == requests.end() || !req_it->granted) {
            return false; // Lock not found or not granted
        }

        // Remove the lock
        requests.erase(req_it);

        // Clean up empty entries
        if (requests.empty()) {
            lock_table_.erase(it);
        }

        if (config_.collect_statistics) {
            stats_.lock_releases.fetch_add(1);
            stats_.active_locks.fetch_sub(1);
            stats_.lock_table_size.store(lock_table_.size());
        }

        return true;
    }

    std::uint32_t LockPartition::release_transaction_locks(std::uint64_t xid)
    {
        std::unique_lock<std::shared_mutex> lock(lock_table_mutex_);

        std::uint32_t released_count = 0;

        for (auto it = lock_table_.begin(); it != lock_table_.end();) {
            auto& requests = it->second;

            // Remove all locks held by this transaction
            auto req_it = requests.begin();
            while (req_it != requests.end()) {
                if (req_it->holder_xid == xid) {
                    if (req_it->granted) {
                        released_count++;
                        if (config_.collect_statistics) {
                            stats_.lock_releases.fetch_add(1);
                            stats_.active_locks.fetch_sub(1);
                        }
                    }
                    req_it = requests.erase(req_it);
                } else {
                    ++req_it;
                }
            }

            // Clean up empty entries
            if (requests.empty()) {
                it = lock_table_.erase(it);
            } else {
                ++it;
            }
        }

        // Remove all wait-for edges involving this transaction
        {
            std::lock_guard<std::mutex> wf_lock(wait_for_mutex_);
            wait_for_graph_.erase(xid);

            // Remove edges where this transaction is the target
            for (auto& [waiter, holders] : wait_for_graph_) {
                holders.erase(xid);
            }

            // Clean up empty entries
            for (auto it = wait_for_graph_.begin(); it != wait_for_graph_.end();) {
                if (it->second.empty()) {
                    it = wait_for_graph_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        if (config_.collect_statistics) {
            stats_.lock_table_size.store(lock_table_.size());
        }

        return released_count;
    }

    bool LockPartition::would_conflict(std::uint64_t resource_id, FastPathLockType lock_type,
                                       std::uint64_t xid) const
    {
        std::shared_lock<std::shared_mutex> lock(lock_table_mutex_);

        auto it = lock_table_.find(resource_id);
        if (it == lock_table_.end()) {
            return false; // No existing locks
        }

        const auto& requests = it->second;
        for (const auto& req : requests) {
            if (req.granted && req.holder_xid != xid && locks_conflict(req.lock_type, lock_type)) {
                return true; // Conflict detected
            }
        }

        return false; // No conflicts
    }

    std::pair<FastPathLockType, std::uint64_t>
    LockPartition::get_lock_info(std::uint64_t resource_id) const
    {
        std::shared_lock<std::shared_mutex> lock(lock_table_mutex_);

        auto it = lock_table_.find(resource_id);
        if (it == lock_table_.end()) {
            return {FastPathLockType::NONE, 0};
        }

        const auto& requests = it->second;
        for (const auto& req : requests) {
            if (req.granted) {
                return {req.lock_type, req.holder_xid};
            }
        }

        return {FastPathLockType::NONE, 0};
    }

    DeadlockInfo LockPartition::check_for_deadlocks()
    {
        DeadlockInfo info;

        if (!config_.deadlock_detection_enabled) {
            return info;
        }

        std::lock_guard<std::mutex> lock(wait_for_mutex_);

        if (config_.collect_statistics) {
            stats_.deadlock_checks.fetch_add(1);
        }

        std::unordered_set<std::uint64_t> visited;
        std::unordered_set<std::uint64_t> rec_stack;

        for (const auto& [waiter_xid, holders] : wait_for_graph_) {
            if (visited.find(waiter_xid) == visited.end()) {
                auto cycle = detect_cycle(waiter_xid, visited, rec_stack);
                if (!cycle.empty()) {
                    info.detected = true;
                    info.cycle = cycle;
                    info.victim_xid = choose_deadlock_victim(cycle);

                    std::ostringstream oss;
                    oss << "Deadlock detected in partition " << partition_id_
                        << " involving transactions: ";
                    for (size_t i = 0; i < cycle.size(); ++i) {
                        if (i > 0)
                            oss << " -> ";
                        oss << cycle[i];
                    }
                    oss << " -> " << cycle[0]; // Complete the cycle
                    oss << ". Victim: " << info.victim_xid;
                    info.description = oss.str();

                    if (config_.collect_statistics) {
                        stats_.deadlocks_detected.fetch_add(1);
                        stats_.deadlock_victims.fetch_add(1);
                    }

                    break; // Handle one deadlock at a time
                }
            }
        }

        return info;
    }

    void LockPartition::add_wait_for_edge(std::uint64_t waiter_xid, std::uint64_t holder_xid)
    {
        std::lock_guard<std::mutex> lock(wait_for_mutex_);
        wait_for_graph_[waiter_xid].insert(holder_xid);
    }

    void LockPartition::remove_wait_for_edge(std::uint64_t waiter_xid, std::uint64_t holder_xid)
    {
        std::lock_guard<std::mutex> lock(wait_for_mutex_);

        auto it = wait_for_graph_.find(waiter_xid);
        if (it != wait_for_graph_.end()) {
            it->second.erase(holder_xid);
            if (it->second.empty()) {
                wait_for_graph_.erase(it);
            }
        }
    }

    PartitionStatistics LockPartition::get_statistics() const
    {
        return stats_;
    }

    void LockPartition::reset_statistics()
    {
        stats_.reset();
    }

    double LockPartition::get_utilization() const
    {
        std::shared_lock<std::shared_mutex> lock(lock_table_mutex_);

        if (config_.max_locks_per_partition == 0) {
            return 0.0;
        }

        return static_cast<double>(lock_table_.size()) / config_.max_locks_per_partition;
    }

    std::uint32_t LockPartition::get_active_lock_count() const
    {
        return stats_.active_locks.load();
    }

    std::uint32_t LockPartition::get_wait_for_graph_size() const
    {
        std::lock_guard<std::mutex> lock(wait_for_mutex_);
        return wait_for_graph_.size();
    }

    std::uint32_t LockPartition::cleanup_expired_entries()
    {
        // This is a simple implementation - in a real system, you'd have
        // more sophisticated cleanup based on transaction state
        return 0;
    }

    bool LockPartition::resize_if_needed()
    {
        std::shared_lock<std::shared_mutex> lock(lock_table_mutex_);

        double current_load = static_cast<double>(lock_table_.size()) / lock_table_.bucket_count();
        return current_load < config_.load_factor_threshold;
    }

    // Private helper methods

    bool LockPartition::locks_conflict(FastPathLockType held_type,
                                       FastPathLockType requested_type) const
    {
        return lock_conflicts(held_type, requested_type);
    }

    std::vector<LockRequest>::iterator LockPartition::find_lock(std::vector<LockRequest>& requests,
                                                                std::uint64_t xid)
    {
        return std::find_if(requests.begin(), requests.end(),
                            [xid](const LockRequest& req) { return req.holder_xid == xid; });
    }

    std::vector<std::uint64_t>
    LockPartition::detect_cycle(std::uint64_t start_xid, std::unordered_set<std::uint64_t>& visited,
                                std::unordered_set<std::uint64_t>& rec_stack) const
    {
        visited.insert(start_xid);
        rec_stack.insert(start_xid);

        auto it = wait_for_graph_.find(start_xid);
        if (it != wait_for_graph_.end()) {
            for (auto holder_xid : it->second) {
                if (rec_stack.find(holder_xid) != rec_stack.end()) {
                    // Cycle detected
                    std::vector<std::uint64_t> cycle;
                    cycle.push_back(start_xid);
                    cycle.push_back(holder_xid);
                    return cycle;
                }

                if (visited.find(holder_xid) == visited.end()) {
                    auto cycle = detect_cycle(holder_xid, visited, rec_stack);
                    if (!cycle.empty()) {
                        cycle.insert(cycle.begin(), start_xid);
                        return cycle;
                    }
                }
            }
        }

        rec_stack.erase(start_xid);
        return {};
    }

    std::uint64_t
    LockPartition::choose_deadlock_victim(const std::vector<std::uint64_t>& cycle) const
    {
        // Simple strategy: choose the transaction with the highest ID (youngest)
        return *std::max_element(cycle.begin(), cycle.end());
    }

    void LockPartition::update_wait_time_stats(std::uint64_t wait_time_us)
    {
        stats_.total_wait_time_us.fetch_add(wait_time_us);
        auto waits = stats_.lock_waits.load();
        if (waits > 0) {
            stats_.avg_wait_time_us.store(stats_.total_wait_time_us.load() / waits);
        }
    }

    // ============================================================================
    // PartitionedLockManager Implementation
    // ============================================================================

    PartitionedLockManager::PartitionedLockManager(const PartitionedLockManagerConfig& config)
        : config_(config)
    {
        assert(config_.is_valid());
    }

    PartitionedLockManager::~PartitionedLockManager()
    {
        shutdown();
    }

    bool PartitionedLockManager::initialize()
    {
        // Create partitions
        partitions_.reserve(config_.partition_count);
        for (std::uint32_t i = 0; i < config_.partition_count; ++i) {
            partitions_.emplace_back(std::make_unique<LockPartition>(i, config_));
        }

        // Start background threads (disabled for testing)
        // if (config_.deadlock_detection_enabled) {
        //     deadlock_detector_thread_ =
        //     std::make_unique<std::thread>(&PartitionedLockManager::deadlock_detector_main, this);
        // }

        // maintenance_thread_ =
        // std::make_unique<std::thread>(&PartitionedLockManager::maintenance_thread_main, this);

        return true;
    }

    void PartitionedLockManager::shutdown()
    {
        shutdown_requested_.store(true);

        if (deadlock_detector_thread_ && deadlock_detector_thread_->joinable()) {
            deadlock_detector_thread_->join();
        }

        if (maintenance_thread_ && maintenance_thread_->joinable()) {
            maintenance_thread_->join();
        }

        partitions_.clear();
    }

    bool PartitionedLockManager::try_lock(std::uint64_t resource_id, FastPathLockType lock_type,
                                          std::uint64_t xid)
    {
        auto partition_id = get_partition_id(resource_id);
        return partitions_[partition_id]->try_lock(resource_id, lock_type, xid);
    }

    bool PartitionedLockManager::acquire_lock(std::uint64_t resource_id, FastPathLockType lock_type,
                                              std::uint64_t xid, std::chrono::milliseconds timeout)
    {
        auto partition_id = get_partition_id(resource_id);
        return partitions_[partition_id]->acquire_lock(resource_id, lock_type, xid, timeout);
    }

    bool PartitionedLockManager::release_lock(std::uint64_t resource_id, std::uint64_t xid)
    {
        auto partition_id = get_partition_id(resource_id);
        return partitions_[partition_id]->release_lock(resource_id, xid);
    }

    std::uint32_t PartitionedLockManager::release_transaction_locks(std::uint64_t xid)
    {
        std::uint32_t total_released = 0;

        // Release locks from all partitions (transaction may hold locks across partitions)
        for (auto& partition : partitions_) {
            total_released += partition->release_transaction_locks(xid);
        }

        return total_released;
    }

    bool PartitionedLockManager::would_conflict(std::uint64_t resource_id,
                                                FastPathLockType lock_type, std::uint64_t xid) const
    {
        auto partition_id = get_partition_id(resource_id);
        return partitions_[partition_id]->would_conflict(resource_id, lock_type, xid);
    }

    std::pair<FastPathLockType, std::uint64_t>
    PartitionedLockManager::get_lock_info(std::uint64_t resource_id) const
    {
        auto partition_id = get_partition_id(resource_id);
        return partitions_[partition_id]->get_lock_info(resource_id);
    }

    std::vector<DeadlockInfo> PartitionedLockManager::check_for_deadlocks()
    {
        std::vector<DeadlockInfo> deadlocks;

        // Check each partition for deadlocks
        for (auto& partition : partitions_) {
            auto deadlock = partition->check_for_deadlocks();
            if (deadlock.detected) {
                deadlocks.push_back(deadlock);
            }
        }

        // Check for cross-partition deadlocks if enabled
        if (config_.cross_partition_deadlock_detection) {
            auto cross_partition_deadlock = detect_cross_partition_deadlock();
            if (cross_partition_deadlock.detected) {
                deadlocks.push_back(cross_partition_deadlock);
            }
        }

        return deadlocks;
    }

    void PartitionedLockManager::set_cross_partition_deadlock_detection(bool enabled)
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        config_.cross_partition_deadlock_detection = enabled;
    }

    bool PartitionedLockManager::update_config(const PartitionedLockManagerConfig& config)
    {
        if (!config.is_valid()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(config_mutex_);

        // Some configuration changes require restart (e.g., partition count)
        // For now, only allow changes to runtime parameters
        config_.deadlock_detection_enabled = config.deadlock_detection_enabled;
        config_.deadlock_check_interval_ms = config.deadlock_check_interval_ms;
        config_.max_wait_time_ms = config.max_wait_time_ms;
        config_.collect_statistics = config.collect_statistics;
        config_.cross_partition_deadlock_detection = config.cross_partition_deadlock_detection;

        return true;
    }

    PartitionedLockManagerConfig PartitionedLockManager::get_config() const
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return config_;
    }

    PartitionStatistics PartitionedLockManager::get_aggregated_statistics() const
    {
        PartitionStatistics aggregated;

        for (const auto& partition : partitions_) {
            auto stats = partition->get_statistics();

            aggregated.lock_requests.fetch_add(stats.lock_requests.load());
            aggregated.lock_grants.fetch_add(stats.lock_grants.load());
            aggregated.lock_conflicts.fetch_add(stats.lock_conflicts.load());
            aggregated.lock_releases.fetch_add(stats.lock_releases.load());
            aggregated.lock_waits.fetch_add(stats.lock_waits.load());
            aggregated.deadlock_checks.fetch_add(stats.deadlock_checks.load());
            aggregated.deadlocks_detected.fetch_add(stats.deadlocks_detected.load());
            aggregated.deadlock_victims.fetch_add(stats.deadlock_victims.load());
            aggregated.total_wait_time_us.fetch_add(stats.total_wait_time_us.load());
            aggregated.active_locks.fetch_add(stats.active_locks.load());
            aggregated.lock_table_size.fetch_add(stats.lock_table_size.load());

            // Update max values
            auto max_locks = stats.max_locks_held.load();
            auto current_max = aggregated.max_locks_held.load();
            if (max_locks > current_max) {
                aggregated.max_locks_held.store(max_locks);
            }
        }

        // Calculate average wait time
        auto total_waits = aggregated.lock_waits.load();
        if (total_waits > 0) {
            aggregated.avg_wait_time_us.store(aggregated.total_wait_time_us.load() / total_waits);
        }

        return aggregated;
    }

    std::vector<PartitionStatistics> PartitionedLockManager::get_per_partition_statistics() const
    {
        std::vector<PartitionStatistics> stats;
        stats.reserve(partitions_.size());

        for (const auto& partition : partitions_) {
            stats.push_back(partition->get_statistics());
        }

        return stats;
    }

    void PartitionedLockManager::reset_statistics()
    {
        for (auto& partition : partitions_) {
            partition->reset_statistics();
        }
    }

    double PartitionedLockManager::get_system_utilization() const
    {
        double total_utilization = 0.0;

        for (const auto& partition : partitions_) {
            total_utilization += partition->get_utilization();
        }

        return total_utilization / partitions_.size();
    }

    std::uint64_t PartitionedLockManager::get_total_active_lock_count() const
    {
        std::uint64_t total_locks = 0;

        for (const auto& partition : partitions_) {
            total_locks += partition->get_active_lock_count();
        }

        return total_locks;
    }

    std::string PartitionedLockManager::generate_performance_report() const
    {
        auto aggregated = get_aggregated_statistics();

        std::ostringstream oss;
        oss << "=== Partitioned Lock Manager Performance Report ===\n";
        oss << "Partition Count: " << partitions_.size() << "\n";
        oss << "Total Lock Requests: " << aggregated.lock_requests.load() << "\n";
        oss << "Total Lock Grants: " << aggregated.lock_grants.load() << "\n";
        oss << "Total Lock Conflicts: " << aggregated.lock_conflicts.load() << "\n";
        oss << "Total Lock Releases: " << aggregated.lock_releases.load() << "\n";
        oss << "Total Lock Waits: " << aggregated.lock_waits.load() << "\n";
        oss << "Active Locks: " << aggregated.active_locks.load() << "\n";
        oss << "Max Locks Held: " << aggregated.max_locks_held.load() << "\n";
        oss << "Average Wait Time: " << aggregated.avg_wait_time_us.load() << " μs\n";
        oss << "System Utilization: " << (get_system_utilization() * 100.0) << "%\n";
        oss << "Deadlocks Detected: " << aggregated.deadlocks_detected.load() << "\n";
        oss << "Deadlock Victims: " << aggregated.deadlock_victims.load() << "\n";

        // Per-partition breakdown
        oss << "\n=== Per-Partition Statistics ===\n";
        for (size_t i = 0; i < partitions_.size(); ++i) {
            auto stats = partitions_[i]->get_statistics();
            oss << "Partition " << i << ":\n";
            oss << "  Requests: " << stats.lock_requests.load() << "\n";
            oss << "  Active Locks: " << stats.active_locks.load() << "\n";
            oss << "  Utilization: " << (partitions_[i]->get_utilization() * 100.0) << "%\n";
            oss << "  Wait-for Graph Size: " << partitions_[i]->get_wait_for_graph_size() << "\n";
        }

        return oss.str();
    }

    std::uint32_t PartitionedLockManager::perform_maintenance()
    {
        std::uint32_t total_cleaned = 0;

        for (auto& partition : partitions_) {
            total_cleaned += partition->cleanup_expired_entries();
            partition->resize_if_needed();
        }

        return total_cleaned;
    }

    std::uint32_t PartitionedLockManager::get_partition_id(std::uint64_t resource_id) const
    {
        return hash_to_partition(resource_id);
    }

    // Private methods

    void PartitionedLockManager::deadlock_detector_main()
    {
        while (!shutdown_requested_.load()) {
            try {
                auto deadlocks = check_for_deadlocks();

                // Handle detected deadlocks
                for (const auto& deadlock : deadlocks) {
                    // In a real implementation, you would abort the victim transaction
                    // For now, just log the deadlock
                    (void)deadlock; // Suppress unused variable warning
                    // LOG(INFO) << deadlock.description;
                }

            } catch (const std::exception& e) {
                // LOG(ERROR) << "Error in deadlock detector: " << e.what();
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(config_.deadlock_check_interval_ms));
        }
    }

    void PartitionedLockManager::maintenance_thread_main()
    {
        while (!shutdown_requested_.load()) {
            try {
                perform_maintenance();
            } catch (const std::exception& e) {
                // LOG(ERROR) << "Error in maintenance thread: " << e.what();
            }

            std::this_thread::sleep_for(std::chrono::minutes(1)); // Run maintenance every minute
        }
    }

    std::uint32_t PartitionedLockManager::hash_to_partition(std::uint64_t resource_id) const
    {
        // Use simple modulo with power-of-2 partition count for efficient bit masking
        return resource_id & (config_.partition_count - 1);
    }

    DeadlockInfo PartitionedLockManager::detect_cross_partition_deadlock()
    {
        // This is a simplified implementation of cross-partition deadlock detection
        // A full implementation would need more sophisticated algorithms
        DeadlockInfo info;

        // Collect global wait-for graph
        auto global_graph = collect_global_wait_for_graph();

        // Simple cycle detection in global graph
        std::unordered_set<std::uint64_t> visited;
        std::unordered_set<std::uint64_t> rec_stack;

        for (const auto& [waiter_xid, holders] : global_graph) {
            if (visited.find(waiter_xid) == visited.end()) {
                // Simplified cycle detection - in reality this would be more complex
                for (auto holder_xid : holders) {
                    if (global_graph.find(holder_xid) != global_graph.end()) {
                        const auto& holder_deps = global_graph.at(holder_xid);
                        if (holder_deps.find(waiter_xid) != holder_deps.end()) {
                            // Simple 2-node cycle detected
                            info.detected = true;
                            info.cycle = {waiter_xid, holder_xid};
                            info.victim_xid = std::max(waiter_xid, holder_xid); // Choose higher ID

                            std::ostringstream oss;
                            oss << "Cross-partition deadlock detected: " << waiter_xid << " -> "
                                << holder_xid << " -> " << waiter_xid;
                            oss << ". Victim: " << info.victim_xid;
                            info.description = oss.str();

                            return info;
                        }
                    }
                }
            }
        }

        return info;
    }

    std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>>
    PartitionedLockManager::collect_global_wait_for_graph() const
    {

        std::unordered_map<std::uint64_t, std::unordered_set<std::uint64_t>> global_graph;

        // This is a simplified implementation - in reality you'd need more sophisticated
        // mechanisms to collect wait-for information across partitions
        return global_graph;
    }

    // ============================================================================
    // Helper Functions
    // ============================================================================

    std::uint32_t calculate_optimal_partition_count(std::uint32_t core_count)
    {
        if (core_count == 0) {
            core_count = std::thread::hardware_concurrency();
        }

        // Rule of thumb: 2-4 partitions per core for lock-heavy workloads
        std::uint32_t target_partitions = core_count * 2;

        // Ensure it's a power of 2 for efficient hashing
        std::uint32_t power_of_2 = 1;
        while (power_of_2 < target_partitions) {
            power_of_2 <<= 1;
        }

        // Cap at reasonable maximum
        return std::min(power_of_2, 128u);
    }

    bool validate_partition_config(const PartitionedLockManagerConfig& config)
    {
        return config.is_valid();
    }

    PartitionedLockManagerConfig create_default_partition_config()
    {
        PartitionedLockManagerConfig config;
        config.partition_count = calculate_optimal_partition_count();
        return config;
    }

} // namespace scratchbird::engine

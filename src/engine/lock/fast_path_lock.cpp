// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/fast_path_lock.h"

#include <algorithm>
#include <chrono>
#include <sstream>
#include <thread>

namespace scratchbird::engine
{

    // ============================================================================
    // FastPathLockArray Implementation
    // ============================================================================

    FastPathLockArray::FastPathLockArray(std::uint32_t max_locks)
        : max_locks_(max_locks), entries_(std::make_unique<FastPathLockEntry[]>(max_locks))
    {
    }

    bool FastPathLockArray::try_acquire_lock(std::uint64_t resource_id, FastPathLockType lock_type,
                                             std::uint64_t xid)
    {
        std::lock_guard<std::mutex> lock(array_mutex_);

        // Check if transaction already holds a lock on this resource
        auto existing_slot = find_slot(resource_id, xid);
        if (existing_slot >= 0) {
            auto& entry = entries_[existing_slot];
            auto current_type = entry.lock_type.load(std::memory_order_relaxed);

            // If requesting same or compatible lock, increment reference count
            if (current_type == lock_type || !lock_conflicts(current_type, lock_type)) {
                entry.ref_count.fetch_add(1, std::memory_order_relaxed);
                return true;
            }

            // Lock upgrade/downgrade not supported in fast-path
            return false;
        }

        // Check for conflicts with other transactions
        if (has_conflicts(resource_id, lock_type, xid)) {
            return false;
        }

        // Find free slot
        auto free_slot = find_free_slot();
        if (free_slot < 0) {
            // Array is full
            return false;
        }

        // Acquire the lock
        entries_[free_slot].initialize(resource_id, lock_type, xid);
        return true;
    }

    bool FastPathLockArray::release_lock(std::uint64_t resource_id, std::uint64_t xid)
    {
        std::lock_guard<std::mutex> lock(array_mutex_);

        auto slot = find_slot(resource_id, xid);
        if (slot < 0) {
            return false; // Lock not found
        }

        auto& entry = entries_[slot];
        auto ref_count = entry.ref_count.fetch_sub(1, std::memory_order_relaxed);

        // If reference count reaches zero, clear the entry
        if (ref_count <= 1) {
            entry.clear();
        }

        return true;
    }

    bool FastPathLockArray::can_acquire_lock(std::uint64_t resource_id, FastPathLockType lock_type,
                                             std::uint64_t xid) const
    {
        std::lock_guard<std::mutex> lock(array_mutex_);

        // Check if transaction already holds this lock
        auto existing_slot = find_slot(resource_id, xid);
        if (existing_slot >= 0) {
            auto current_type = entries_[existing_slot].lock_type.load(std::memory_order_relaxed);
            return current_type == lock_type || !lock_conflicts(current_type, lock_type);
        }

        // Check for conflicts and available slots
        return !has_conflicts(resource_id, lock_type, xid) && find_free_slot() >= 0;
    }

    std::pair<FastPathLockType, std::uint64_t>
    FastPathLockArray::get_lock_info(std::uint64_t resource_id) const
    {
        std::lock_guard<std::mutex> lock(array_mutex_);

        for (std::uint32_t i = 0; i < max_locks_; ++i) {
            const auto& entry = entries_[i];
            if (entry.resource_id.load(std::memory_order_relaxed) == resource_id &&
                !entry.is_free()) {
                return {entry.lock_type.load(std::memory_order_relaxed),
                        entry.holder_xid.load(std::memory_order_relaxed)};
            }
        }

        return {FastPathLockType::NONE, 0};
    }

    std::uint32_t FastPathLockArray::clear_transaction_locks(std::uint64_t xid)
    {
        std::lock_guard<std::mutex> lock(array_mutex_);

        std::uint32_t cleared_count = 0;
        for (std::uint32_t i = 0; i < max_locks_; ++i) {
            auto& entry = entries_[i];
            if (entry.holder_xid.load(std::memory_order_relaxed) == xid && !entry.is_free()) {
                entry.clear();
                cleared_count++;
            }
        }

        return cleared_count;
    }

    std::uint32_t FastPathLockArray::get_transaction_lock_count(std::uint64_t xid) const
    {
        std::lock_guard<std::mutex> lock(array_mutex_);

        std::uint32_t count = 0;
        for (std::uint32_t i = 0; i < max_locks_; ++i) {
            const auto& entry = entries_[i];
            if (entry.holder_xid.load(std::memory_order_relaxed) == xid && !entry.is_free()) {
                count++;
            }
        }

        return count;
    }

    std::uint32_t FastPathLockArray::cleanup_expired_locks(std::uint64_t max_age_seconds)
    {
        std::lock_guard<std::mutex> lock(array_mutex_);

        auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
        auto max_age_us = max_age_seconds * 1000000ULL;

        std::uint32_t cleaned_count = 0;
        for (std::uint32_t i = 0; i < max_locks_; ++i) {
            auto& entry = entries_[i];
            if (!entry.is_free()) {
                auto acquire_time = entry.acquire_timestamp.load(std::memory_order_relaxed);
                if (now - acquire_time > max_age_us) {
                    entry.clear();
                    cleaned_count++;
                }
            }
        }

        return cleaned_count;
    }

    double FastPathLockArray::get_utilization() const
    {
        std::lock_guard<std::mutex> lock(array_mutex_);
        return static_cast<double>(get_used_slots()) / max_locks_;
    }

    std::uint32_t FastPathLockArray::get_used_slots() const
    {
        std::uint32_t used = 0;
        for (std::uint32_t i = 0; i < max_locks_; ++i) {
            if (!entries_[i].is_free()) {
                used++;
            }
        }
        return used;
    }

    bool FastPathLockArray::is_full() const
    {
        std::lock_guard<std::mutex> lock(array_mutex_);
        return find_free_slot() < 0;
    }

    std::int32_t FastPathLockArray::find_free_slot() const
    {
        for (std::uint32_t i = 0; i < max_locks_; ++i) {
            if (entries_[i].is_free()) {
                return static_cast<std::int32_t>(i);
            }
        }
        return -1;
    }

    std::int32_t FastPathLockArray::find_slot(std::uint64_t resource_id, std::uint64_t xid) const
    {
        for (std::uint32_t i = 0; i < max_locks_; ++i) {
            if (entries_[i].matches(resource_id, xid)) {
                return static_cast<std::int32_t>(i);
            }
        }
        return -1;
    }

    bool FastPathLockArray::has_conflicts(std::uint64_t resource_id, FastPathLockType lock_type,
                                          std::uint64_t xid) const
    {
        // Check if this is a row-level lock (row_id != 0)
        bool is_row_level = get_row_id_from_resource_id(resource_id) != 0;

        for (std::uint32_t i = 0; i < max_locks_; ++i) {
            const auto& entry = entries_[i];
            if (entry.resource_id.load(std::memory_order_relaxed) == resource_id &&
                !entry.is_free() && entry.holder_xid.load(std::memory_order_relaxed) != xid) {

                auto held_type = entry.lock_type.load(std::memory_order_relaxed);

                // For row-level locks, be more restrictive - any existing lock conflicts
                // unless both are compatible read locks
                if (is_row_level) {
                    // Row-level: only allow multiple ACCESS_SHARE or ROW_SHARE locks
                    if (held_type == FastPathLockType::ACCESS_SHARE_LOCK &&
                        lock_type == FastPathLockType::ACCESS_SHARE_LOCK) {
                        continue; // Compatible
                    }
                    if (held_type == FastPathLockType::ROW_SHARE_LOCK &&
                        lock_type == FastPathLockType::ROW_SHARE_LOCK) {
                        continue; // Compatible
                    }
                    // All other combinations conflict at row level
                    return true;
                } else {
                    // Table-level: use standard conflict matrix
                    if (lock_conflicts(held_type, lock_type)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // ============================================================================
    // FastPathLockManager Implementation
    // ============================================================================

    FastPathLockManager::FastPathLockManager(const FastPathLockConfig& config) : config_(config) {}

    FastPathLockManager::~FastPathLockManager()
    {
        shutdown();
    }

    bool FastPathLockManager::initialize()
    {
        std::lock_guard<std::mutex> config_lock(config_mutex_);

        if (!config_.is_valid()) {
            return false;
        }

        // Start background cleanup thread if automatic cleanup is enabled
        if (config_.enable_automatic_cleanup && config_.cleanup_interval_seconds > 0) {
            shutdown_requested_.store(false);
            cleanup_thread_ =
                std::make_unique<std::thread>(&FastPathLockManager::cleanup_thread_main, this);
        }

        return true;
    }

    void FastPathLockManager::shutdown()
    {
        shutdown_requested_.store(true);

        if (cleanup_thread_ && cleanup_thread_->joinable()) {
            cleanup_thread_->join();
            cleanup_thread_.reset();
        }

        // Clear all thread arrays
        std::unique_lock<std::shared_mutex> arrays_lock(arrays_mutex_);
        thread_arrays_.clear();
    }

    bool FastPathLockManager::try_fast_path_lock(std::uint64_t resource_id,
                                                 FastPathLockType lock_type, std::uint64_t xid)
    {
        if (!config_.enabled || !is_fast_path_suitable(lock_type)) {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            global_stats_.fallback_count.fetch_add(1);
            return false;
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        // Update attempt statistics
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            global_stats_.fast_path_attempts.fetch_add(1);
            global_stats_.lock_type_counts[static_cast<int>(lock_type)].fetch_add(1);
        }

        // Get lock array for current thread
        auto* array = get_current_thread_array();
        if (!array) {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            global_stats_.fallback_count.fetch_add(1);
            return false;
        }

        // Try to acquire the lock
        bool success = array->try_acquire_lock(resource_id, lock_type, xid);

        // Update statistics
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            if (success) {
                global_stats_.fast_path_successes.fetch_add(1);
                global_stats_.update_timing(duration.count());
            } else {
                global_stats_.conflict_count.fetch_add(1);
                if (array->is_full()) {
                    global_stats_.overflow_count.fetch_add(1);
                }
            }
        }

        return success;
    }

    bool FastPathLockManager::release_fast_path_lock(std::uint64_t resource_id, std::uint64_t xid)
    {
        if (!config_.enabled) {
            return false;
        }

        auto* array = get_current_thread_array();
        if (!array) {
            return false;
        }

        bool success = array->release_lock(resource_id, xid);

        if (success) {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            global_stats_.fast_path_releases.fetch_add(1);
        }

        return success;
    }

    bool FastPathLockManager::is_fast_path_eligible(std::uint64_t resource_id,
                                                    FastPathLockType lock_type,
                                                    std::uint64_t xid) const
    {
        if (!config_.enabled || !is_fast_path_suitable(lock_type)) {
            return false;
        }

        auto* array = get_current_thread_array_const();
        if (array == nullptr) {
            // Thread not registered yet, but lock type is suitable, so eligible
            return true;
        }

        return array->can_acquire_lock(resource_id, lock_type, xid);
    }

    std::uint32_t FastPathLockManager::clear_transaction_locks(std::uint64_t xid)
    {
        std::shared_lock<std::shared_mutex> arrays_lock(arrays_mutex_);

        std::uint32_t total_cleared = 0;
        for (auto& [thread_id, array] : thread_arrays_) {
            if (array) {
                total_cleared += array->clear_transaction_locks(xid);
            }
        }

        return total_cleared;
    }

    bool FastPathLockManager::register_backend(std::thread::id thread_id)
    {
        std::unique_lock<std::shared_mutex> arrays_lock(arrays_mutex_);

        if (thread_arrays_.find(thread_id) != thread_arrays_.end()) {
            return false; // Already registered
        }

        std::lock_guard<std::mutex> config_lock(config_mutex_);
        thread_arrays_[thread_id] =
            std::make_unique<FastPathLockArray>(config_.max_fast_path_locks);
        return true;
    }

    void FastPathLockManager::unregister_backend(std::thread::id thread_id)
    {
        std::unique_lock<std::shared_mutex> arrays_lock(arrays_mutex_);
        thread_arrays_.erase(thread_id);
    }

    FastPathLockArray* FastPathLockManager::get_current_thread_array()
    {
        auto current_thread_id = std::this_thread::get_id();

        {
            std::shared_lock<std::shared_mutex> arrays_lock(arrays_mutex_);
            auto it = thread_arrays_.find(current_thread_id);
            if (it != thread_arrays_.end()) {
                return it->second.get();
            }
        }

        // Auto-register current thread
        register_backend(current_thread_id);

        std::shared_lock<std::shared_mutex> arrays_lock(arrays_mutex_);
        auto it = thread_arrays_.find(current_thread_id);
        return it != thread_arrays_.end() ? it->second.get() : nullptr;
    }

    const FastPathLockArray* FastPathLockManager::get_current_thread_array_const() const
    {
        auto current_thread_id = std::this_thread::get_id();

        std::shared_lock<std::shared_mutex> arrays_lock(arrays_mutex_);
        auto it = thread_arrays_.find(current_thread_id);
        return it != thread_arrays_.end() ? it->second.get() : nullptr;
    }

    bool FastPathLockManager::update_config(const FastPathLockConfig& config)
    {
        if (!config.is_valid()) {
            return false;
        }

        std::lock_guard<std::mutex> config_lock(config_mutex_);
        config_ = config;
        return true;
    }

    FastPathLockConfig FastPathLockManager::get_config() const
    {
        std::lock_guard<std::mutex> config_lock(config_mutex_);
        return config_;
    }

    FastPathLockStats FastPathLockManager::get_statistics() const
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        return global_stats_;
    }

    void FastPathLockManager::reset_statistics()
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        global_stats_.reset();
    }

    std::unordered_map<std::thread::id, std::uint32_t>
    FastPathLockManager::get_per_thread_lock_counts() const
    {
        std::shared_lock<std::shared_mutex> arrays_lock(arrays_mutex_);

        std::unordered_map<std::thread::id, std::uint32_t> counts;
        for (const auto& [thread_id, array] : thread_arrays_) {
            if (array) {
                counts[thread_id] = array->get_used_slots();
            }
        }

        return counts;
    }

    std::uint32_t FastPathLockManager::get_backend_count() const
    {
        std::shared_lock<std::shared_mutex> arrays_lock(arrays_mutex_);
        return static_cast<std::uint32_t>(thread_arrays_.size());
    }

    double FastPathLockManager::get_system_utilization() const
    {
        std::shared_lock<std::shared_mutex> arrays_lock(arrays_mutex_);

        if (thread_arrays_.empty()) {
            return 0.0;
        }

        double total_utilization = 0.0;
        for (const auto& [thread_id, array] : thread_arrays_) {
            if (array) {
                total_utilization += array->get_utilization();
            }
        }

        return total_utilization / thread_arrays_.size();
    }

    std::string FastPathLockManager::generate_performance_report() const
    {
        auto stats = get_statistics();
        auto config = get_config();

        std::ostringstream report;
        report << "Fast-Path Lock Manager Performance Report\n";
        report << "=========================================\n";
        report << "Configuration:\n";
        report << "  Enabled: " << (config.enabled ? "Yes" : "No") << "\n";
        report << "  Max Fast-Path Locks: " << config.max_fast_path_locks << "\n";
        report << "  Statistics Collection: " << (config.collect_statistics ? "Yes" : "No") << "\n";
        report << "  Automatic Cleanup: " << (config.enable_automatic_cleanup ? "Yes" : "No")
               << "\n\n";

        report << "Performance Statistics:\n";
        report << "  Fast-Path Attempts: " << stats.fast_path_attempts.load() << "\n";
        report << "  Fast-Path Successes: " << stats.fast_path_successes.load() << "\n";
        report << "  Success Ratio: " << (stats.get_success_ratio() * 100.0) << "%\n";
        report << "  Fast-Path Releases: " << stats.fast_path_releases.load() << "\n";
        report << "  Fallback Count: " << stats.fallback_count.load() << "\n";
        report << "  Conflict Count: " << stats.conflict_count.load() << "\n";
        report << "  Overflow Count: " << stats.overflow_count.load() << "\n";
        report << "  Average Acquisition Time: " << stats.avg_acquisition_time_us.load()
               << " μs\n\n";

        report << "System Information:\n";
        report << "  Registered Backends: " << get_backend_count() << "\n";
        report << "  System Utilization: " << (get_system_utilization() * 100.0) << "%\n\n";

        report << "Lock Type Distribution:\n";
        for (int i = 1; i < 9; ++i) { // Skip NONE (0)
            auto lock_type = static_cast<FastPathLockType>(i);
            auto count = stats.lock_type_counts[i].load();
            if (count > 0) {
                report << "  " << lock_type_to_string(lock_type) << ": " << count << "\n";
            }
        }

        return report.str();
    }

    void FastPathLockManager::set_monitoring_enabled(bool enabled)
    {
        std::lock_guard<std::mutex> config_lock(config_mutex_);
        config_.collect_statistics = enabled;
    }

    std::uint64_t FastPathLockManager::get_lock_type_count(FastPathLockType lock_type) const
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        auto type_index = static_cast<int>(lock_type);
        if (type_index >= 0 && type_index < 9) {
            return global_stats_.lock_type_counts[type_index].load();
        }
        return 0;
    }

    void FastPathLockManager::cleanup_thread_main()
    {
        while (!shutdown_requested_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(config_.cleanup_interval_seconds));

            if (shutdown_requested_.load()) {
                break;
            }

            // Perform cleanup
            cleanup_all_expired_locks();
        }
    }

    std::uint32_t FastPathLockManager::cleanup_all_expired_locks()
    {
        std::shared_lock<std::shared_mutex> arrays_lock(arrays_mutex_);

        std::uint32_t total_cleaned = 0;
        for (auto& [thread_id, array] : thread_arrays_) {
            if (array) {
                total_cleaned += array->cleanup_expired_locks(config_.max_lock_hold_time_seconds);
            }
        }

        return total_cleaned;
    }

    void FastPathLockManager::update_global_statistics()
    {
        // This could be expanded to aggregate per-thread statistics if needed
        // Currently, statistics are updated directly in the global stats
    }

    // ============================================================================
    // Helper Functions Implementation
    // ============================================================================

    const char* lock_type_to_string(FastPathLockType lock_type)
    {
        switch (lock_type) {
        case FastPathLockType::NONE:
            return "NONE";
        case FastPathLockType::ACCESS_SHARE_LOCK:
            return "ACCESS_SHARE_LOCK";
        case FastPathLockType::ROW_SHARE_LOCK:
            return "ROW_SHARE_LOCK";
        case FastPathLockType::ROW_EXCLUSIVE_LOCK:
            return "ROW_EXCLUSIVE_LOCK";
        case FastPathLockType::SHARE_UPDATE_EXCLUSIVE:
            return "SHARE_UPDATE_EXCLUSIVE";
        case FastPathLockType::SHARE_LOCK:
            return "SHARE_LOCK";
        case FastPathLockType::SHARE_ROW_EXCLUSIVE:
            return "SHARE_ROW_EXCLUSIVE";
        case FastPathLockType::EXCLUSIVE_LOCK:
            return "EXCLUSIVE_LOCK";
        case FastPathLockType::ACCESS_EXCLUSIVE_LOCK:
            return "ACCESS_EXCLUSIVE_LOCK";
        }
        return "UNKNOWN";
    }

    bool is_fast_path_suitable(FastPathLockType lock_type)
    {
        // Fast-path is suitable for common, lightweight locks
        switch (lock_type) {
        case FastPathLockType::ACCESS_SHARE_LOCK:
        case FastPathLockType::ROW_SHARE_LOCK:
        case FastPathLockType::ROW_EXCLUSIVE_LOCK:
            return true;
        case FastPathLockType::SHARE_UPDATE_EXCLUSIVE:
        case FastPathLockType::SHARE_LOCK:
        case FastPathLockType::SHARE_ROW_EXCLUSIVE:
        case FastPathLockType::EXCLUSIVE_LOCK:
        case FastPathLockType::ACCESS_EXCLUSIVE_LOCK:
            return false; // These require regular lock manager
        case FastPathLockType::NONE:
            return false;
        }
        return false;
    }

    std::uint32_t get_lock_type_priority(FastPathLockType lock_type)
    {
        // Higher priority = stronger lock
        switch (lock_type) {
        case FastPathLockType::NONE:
            return 0;
        case FastPathLockType::ACCESS_SHARE_LOCK:
            return 1;
        case FastPathLockType::ROW_SHARE_LOCK:
            return 2;
        case FastPathLockType::ROW_EXCLUSIVE_LOCK:
            return 3;
        case FastPathLockType::SHARE_UPDATE_EXCLUSIVE:
            return 4;
        case FastPathLockType::SHARE_LOCK:
            return 5;
        case FastPathLockType::SHARE_ROW_EXCLUSIVE:
            return 6;
        case FastPathLockType::EXCLUSIVE_LOCK:
            return 7;
        case FastPathLockType::ACCESS_EXCLUSIVE_LOCK:
            return 8;
        }
        return 0;
    }

    std::uint64_t make_resource_id(std::uint32_t table_oid, std::uint32_t row_id)
    {
        return (static_cast<std::uint64_t>(table_oid) << 32) | row_id;
    }

    std::uint32_t get_table_oid_from_resource_id(std::uint64_t resource_id)
    {
        return static_cast<std::uint32_t>(resource_id >> 32);
    }

    std::uint32_t get_row_id_from_resource_id(std::uint64_t resource_id)
    {
        return static_cast<std::uint32_t>(resource_id & 0xFFFFFFFF);
    }

} // namespace scratchbird::engine

// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/plan_cache.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>

namespace scratchbird::engine
{

    // ExecutionStatistics implementation

    void ExecutionStatistics::update_execution_stats(std::uint64_t execution_time_us,
                                                     std::uint64_t rows_processed, bool success)
    {
        execution_count.fetch_add(1);
        total_execution_time_us.fetch_add(execution_time_us);
        total_rows_processed.fetch_add(rows_processed);

        // Update min execution time
        auto current_min = min_execution_time_us.load();
        while (execution_time_us < current_min &&
               !min_execution_time_us.compare_exchange_weak(current_min, execution_time_us)) {
            // Loop until we successfully update or find a smaller value
        }

        // Update max execution time
        auto current_max = max_execution_time_us.load();
        while (execution_time_us > current_max &&
               !max_execution_time_us.compare_exchange_weak(current_max, execution_time_us)) {
            // Loop until we successfully update or find a larger value
        }

        // Update average execution time
        auto count = execution_count.load();
        if (count > 0) {
            avg_execution_time_us.store(total_execution_time_us.load() / count);
        }

        // Update success ratio
        if (!success) {
            // This is a simplified success ratio calculation
            auto current_ratio = success_ratio.load();
            auto new_ratio = (current_ratio * (count - 1) + (success ? 1.0 : 0.0)) / count;
            success_ratio.store(new_ratio);
        }

        // Update last execution time
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        last_execution_time.store(now);
    }

    void ExecutionStatistics::reset()
    {
        execution_count.store(0);
        total_execution_time_us.store(0);
        avg_execution_time_us.store(0);
        min_execution_time_us.store(UINT64_MAX);
        max_execution_time_us.store(0);
        total_rows_processed.store(0);
        invalidation_count.store(0);
        last_execution_time.store(0);
        success_ratio.store(1.0);
    }

    // PlanKey implementation

    PlanKey::PlanKey(const std::string& query) : query_text(normalize_query(query)) {}

    PlanKey::PlanKey(const std::string& query, const std::string& db_name,
                     const std::string& schema_name)
        : query_text(normalize_query(query)), database_name(db_name), schema_name(schema_name)
    {
    }

    bool PlanKey::operator==(const PlanKey& other) const
    {
        return query_text == other.query_text && database_name == other.database_name &&
               schema_name == other.schema_name && parameter_types == other.parameter_types &&
               read_only == other.read_only && transaction_context == other.transaction_context &&
               isolation_level == other.isolation_level && user_name == other.user_name &&
               user_roles == other.user_roles;
    }

    std::size_t PlanKey::hash() const
    {
        if (hash_code == 0) {
            // Compute hash lazily
            std::size_t h1 = std::hash<std::string>{}(query_text);
            std::size_t h2 = std::hash<std::string>{}(database_name);
            std::size_t h3 = std::hash<std::string>{}(schema_name);
            std::size_t h4 = std::hash<std::string>{}(user_name);
            std::size_t h5 = std::hash<bool>{}(read_only);
            std::size_t h6 = std::hash<std::uint32_t>{}(isolation_level);

            // Combine parameter types
            std::size_t param_hash = 0;
            for (const auto& type : parameter_types) {
                param_hash ^= std::hash<std::string>{}(type) + 0x9e3779b9 + (param_hash << 6) +
                              (param_hash >> 2);
            }

            // Combine user roles
            std::size_t role_hash = 0;
            for (const auto& role : user_roles) {
                role_hash ^= std::hash<std::string>{}(role) + 0x9e3779b9 + (role_hash << 6) +
                             (role_hash >> 2);
            }

            // Combine all hashes
            hash_code = h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5) ^
                        (param_hash << 6) ^ (role_hash << 7);
        }
        return hash_code;
    }

    std::string PlanKey::to_string() const
    {
        std::ostringstream oss;
        oss << "PlanKey{query='" << query_text.substr(0, 50);
        if (query_text.length() > 50)
            oss << "...";
        oss << "', db='" << database_name << "', schema='" << schema_name << "', user='"
            << user_name << "', readonly=" << read_only << "}";
        return oss.str();
    }

    std::string PlanKey::normalize_query(const std::string& query)
    {
        if (query.empty())
            return query;

        std::string normalized = query;

        // Remove leading/trailing whitespace
        normalized.erase(0, normalized.find_first_not_of(" \t\n\r"));
        normalized.erase(normalized.find_last_not_of(" \t\n\r") + 1);

        // Convert to uppercase for keywords (simplified normalization)
        // In a real implementation, this would be more sophisticated
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::toupper);

        // Replace multiple whitespace with single space
        std::regex ws_regex("\\s+");
        normalized = std::regex_replace(normalized, ws_regex, " ");

        return normalized;
    }

    // CachedPlan implementation

    CachedPlan::CachedPlan(const PlanKey& key, std::shared_ptr<QueryPlan> plan, CachedPlanType type)
        : key_(key), plan_(plan), type_(type), creation_time_(std::chrono::system_clock::now()),
          last_access_time_(std::chrono::system_clock::now())
    {
        plan_size_bytes_ = calculate_plan_size();
        last_access_time_.store(std::chrono::system_clock::now());
    }

    void CachedPlan::record_access()
    {
        access_count_.fetch_add(1);
        last_access_time_.store(std::chrono::system_clock::now());
    }

    std::size_t CachedPlan::get_memory_footprint() const
    {
        // Calculate approximate memory footprint
        std::size_t size = sizeof(CachedPlan);
        size += key_.query_text.size();
        size += key_.database_name.size();
        size += key_.schema_name.size();
        size += key_.user_name.size();

        for (const auto& param_type : key_.parameter_types) {
            size += param_type.size();
        }

        for (const auto& role : key_.user_roles) {
            size += role.size();
        }

        size += plan_size_bytes_;

        return size;
    }

    std::size_t CachedPlan::calculate_plan_size() const
    {
        // This is a placeholder implementation
        // In a real system, this would traverse the plan tree and calculate actual size
        return 1024; // Assume 1KB per plan for now
    }

    // PlanCacheConfig implementation

    bool PlanCacheConfig::is_valid() const
    {
        return max_plans > 0 && max_memory_bytes > 0 && entry_ttl_seconds > 0 &&
               eviction_threshold > 0.0 && eviction_threshold <= 1.0 &&
               cleanup_interval_seconds > 0;
    }

    std::string PlanCacheConfig::validate() const
    {
        if (max_plans == 0)
            return "max_plans must be greater than 0";
        if (max_memory_bytes == 0)
            return "max_memory_bytes must be greater than 0";
        if (entry_ttl_seconds == 0)
            return "entry_ttl_seconds must be greater than 0";
        if (eviction_threshold <= 0.0 || eviction_threshold > 1.0)
            return "eviction_threshold must be between 0.0 and 1.0";
        if (cleanup_interval_seconds == 0)
            return "cleanup_interval_seconds must be greater than 0";
        return "";
    }

    // PlanCache implementation

    PlanCache::PlanCache(const PlanCacheConfig& config) : config_(config)
    {
        // Initialize LRU list with dummy head and tail nodes
        lru_head_ = std::make_shared<LRUNode>(0, nullptr);
        lru_tail_ = std::make_shared<LRUNode>(0, nullptr);
        lru_head_->next = lru_tail_;
        lru_tail_->prev = lru_head_;
    }

    PlanCache::~PlanCache()
    {
        shutdown();
    }

    bool PlanCache::initialize()
    {
        if (!config_.is_valid()) {
            return false;
        }

        // Start background cleanup thread
        if (config_.cleanup_interval_seconds > 0) {
            cleanup_thread_ = std::make_unique<std::thread>(&PlanCache::cleanup_thread_main, this);
        }

        return true;
    }

    void PlanCache::shutdown()
    {
        shutdown_requested_.store(true);

        if (cleanup_thread_ && cleanup_thread_->joinable()) {
            cleanup_thread_->join();
        }

        clear();
    }

    bool PlanCache::insert_plan(const PlanKey& key, std::shared_ptr<QueryPlan> plan,
                                CachedPlanType type)
    {
        if (!config_.enabled || !plan) {
            return false;
        }

        auto start_time = std::chrono::steady_clock::now();

        auto cached_plan = std::make_shared<CachedPlan>(key, plan, type);
        std::size_t key_hash = key.hash();

        {
            std::unique_lock<std::shared_mutex> cache_lock(cache_mutex_);
            std::lock_guard<std::mutex> lru_lock(lru_mutex_);

            // Check if plan already exists
            auto it = cache_.find(key_hash);
            if (it != cache_.end()) {
                // Update existing plan
                it->second = cached_plan;

                // Move to head of LRU
                auto lru_it = lru_map_.find(key_hash);
                if (lru_it != lru_map_.end()) {
                    lru_move_to_head(lru_it->second);
                }
            } else {
                // Check if we need to evict
                if (should_evict()) {
                    evict_plans(1);
                }

                // Insert new plan
                cache_[key_hash] = cached_plan;

                // Add to LRU
                auto lru_node = std::make_shared<LRUNode>(key_hash, cached_plan);
                lru_map_[key_hash] = lru_node;
                lru_add_to_head(lru_node);

                // Update statistics
                {
                    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                    statistics_.total_plans.fetch_add(1);

                    switch (type) {
                    case CachedPlanType::GENERIC_PLAN:
                        statistics_.generic_plans.fetch_add(1);
                        break;
                    case CachedPlanType::SPECIFIC_PLAN:
                        statistics_.specific_plans.fetch_add(1);
                        break;
                    case CachedPlanType::PREPARED_PLAN:
                        statistics_.prepared_plans.fetch_add(1);
                        break;
                    case CachedPlanType::PARAMETERIZED_PLAN:
                        // Could add separate counter if needed
                        break;
                    }
                }
            }
        }

        auto end_time = std::chrono::steady_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        update_insertion_statistics(duration);

        update_memory_usage();

        return true;
    }

    std::shared_ptr<CachedPlan> PlanCache::lookup_plan(const PlanKey& key)
    {
        if (!config_.enabled) {
            update_miss_statistics();
            return nullptr;
        }

        std::size_t key_hash = key.hash();

        {
            std::shared_lock<std::shared_mutex> cache_lock(cache_mutex_);
            std::lock_guard<std::mutex> lru_lock(lru_mutex_);

            auto it = cache_.find(key_hash);
            if (it != cache_.end() && it->second->is_valid()) {
                // Found valid plan
                auto cached_plan = it->second;
                cached_plan->record_access();

                // Move to head of LRU
                auto lru_it = lru_map_.find(key_hash);
                if (lru_it != lru_map_.end()) {
                    lru_move_to_head(lru_it->second);
                }

                update_hit_statistics();
                return cached_plan;
            }
        }

        update_miss_statistics();
        return nullptr;
    }

    bool PlanCache::remove_plan(const PlanKey& key)
    {
        std::size_t key_hash = key.hash();

        std::unique_lock<std::shared_mutex> cache_lock(cache_mutex_);
        std::lock_guard<std::mutex> lru_lock(lru_mutex_);

        auto it = cache_.find(key_hash);
        if (it != cache_.end()) {
            // Remove from LRU
            auto lru_it = lru_map_.find(key_hash);
            if (lru_it != lru_map_.end()) {
                lru_remove_node(lru_it->second);
                lru_map_.erase(lru_it);
            }

            // Remove from cache
            cache_.erase(it);

            // Update statistics
            {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                statistics_.total_plans.fetch_sub(1);
            }

            update_memory_usage();
            return true;
        }

        return false;
    }

    std::uint32_t PlanCache::evict_plans(std::uint32_t max_evictions)
    {
        std::uint32_t evicted_count = 0;

        std::unique_lock<std::shared_mutex> cache_lock(cache_mutex_);
        std::lock_guard<std::mutex> lru_lock(lru_mutex_);

        while (evicted_count < max_evictions && lru_tail_->prev != lru_head_) {
            auto lru_node = lru_remove_tail();
            if (!lru_node || !lru_node->plan) {
                break;
            }

            // Don't evict pinned plans
            if (lru_node->plan->has_flags(CachedPlanFlags::PINNED)) {
                continue;
            }

            // Remove from cache and LRU map
            cache_.erase(lru_node->key_hash);
            lru_map_.erase(lru_node->key_hash);

            evicted_count++;
        }

        if (evicted_count > 0) {
            // Update statistics
            {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                statistics_.cache_evictions.fetch_add(evicted_count);
                statistics_.total_plans.fetch_sub(evicted_count);
            }

            update_memory_usage();
        }

        return evicted_count;
    }

    void PlanCache::clear()
    {
        std::unique_lock<std::shared_mutex> cache_lock(cache_mutex_);
        std::lock_guard<std::mutex> lru_lock(lru_mutex_);

        cache_.clear();
        lru_map_.clear();

        // Reset LRU list
        lru_head_->next = lru_tail_;
        lru_tail_->prev = lru_head_;

        // Reset statistics
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.total_plans.store(0);
            statistics_.generic_plans.store(0);
            statistics_.specific_plans.store(0);
            statistics_.prepared_plans.store(0);
            statistics_.memory_usage_bytes.store(0);
        }
    }

    std::uint32_t PlanCache::invalidate_plans(const std::string& pattern)
    {
        std::uint32_t invalidated_count = 0;
        std::regex pattern_regex(pattern);

        std::shared_lock<std::shared_mutex> cache_lock(cache_mutex_);

        for (auto& [hash, cached_plan] : cache_) {
            if (std::regex_search(cached_plan->get_key().query_text, pattern_regex)) {
                cached_plan->invalidate();
                cached_plan->get_statistics().invalidation_count.fetch_add(1);
                invalidated_count++;
            }
        }

        if (invalidated_count > 0) {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.cache_invalidations.fetch_add(invalidated_count);
        }

        return invalidated_count;
    }

    std::uint32_t PlanCache::invalidate_plans_by_schema(const std::string& database_name,
                                                        const std::string& schema_name)
    {
        std::uint32_t invalidated_count = 0;

        std::shared_lock<std::shared_mutex> cache_lock(cache_mutex_);

        for (auto& [hash, cached_plan] : cache_) {
            const auto& key = cached_plan->get_key();
            if (key.database_name == database_name && key.schema_name == schema_name) {
                cached_plan->invalidate();
                cached_plan->get_statistics().invalidation_count.fetch_add(1);
                invalidated_count++;
            }
        }

        if (invalidated_count > 0) {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.cache_invalidations.fetch_add(invalidated_count);
        }

        return invalidated_count;
    }

    std::uint32_t PlanCache::cleanup_expired_plans()
    {
        std::uint32_t cleaned_count = 0;
        auto now = std::chrono::system_clock::now();
        auto ttl = std::chrono::seconds(config_.entry_ttl_seconds);

        std::vector<std::size_t> expired_hashes;

        {
            std::shared_lock<std::shared_mutex> cache_lock(cache_mutex_);

            for (const auto& [hash, cached_plan] : cache_) {
                if (now - cached_plan->get_creation_time() > ttl) {
                    expired_hashes.push_back(hash);
                }
            }
        }

        // Remove expired plans
        for (std::size_t hash : expired_hashes) {
            std::unique_lock<std::shared_mutex> cache_lock(cache_mutex_);
            std::lock_guard<std::mutex> lru_lock(lru_mutex_);

            auto it = cache_.find(hash);
            if (it != cache_.end()) {
                // Remove from LRU
                auto lru_it = lru_map_.find(hash);
                if (lru_it != lru_map_.end()) {
                    lru_remove_node(lru_it->second);
                    lru_map_.erase(lru_it);
                }

                cache_.erase(it);
                cleaned_count++;
            }
        }

        if (cleaned_count > 0) {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.total_plans.fetch_sub(cleaned_count);
            statistics_.cache_evictions.fetch_add(cleaned_count);

            update_memory_usage();
        }

        return cleaned_count;
    }

    bool PlanCache::update_config(const PlanCacheConfig& config)
    {
        if (!config.is_valid()) {
            return false;
        }

        std::unique_lock<std::shared_mutex> lock(config_mutex_);
        config_ = config;

        // Apply memory limits if reduced
        if (config_.enabled) {
            enforce_memory_limits();
        }

        return true;
    }

    PlanCacheConfig PlanCache::get_config() const
    {
        std::shared_lock<std::shared_mutex> lock(config_mutex_);
        return config_;
    }

    PlanCacheStats PlanCache::get_statistics() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return statistics_;
    }

    void PlanCache::reset_statistics()
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.reset();
    }

    std::vector<PlanKey> PlanCache::get_all_plan_keys() const
    {
        std::vector<PlanKey> keys;

        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        keys.reserve(cache_.size());

        for (const auto& [hash, cached_plan] : cache_) {
            keys.push_back(cached_plan->get_key());
        }

        return keys;
    }

    std::shared_ptr<CachedPlan> PlanCache::get_plan_details(const PlanKey& key) const
    {
        std::size_t key_hash = key.hash();

        std::shared_lock<std::shared_mutex> lock(cache_mutex_);
        auto it = cache_.find(key_hash);

        if (it != cache_.end()) {
            return it->second;
        }

        return nullptr;
    }

    std::uint64_t PlanCache::get_memory_usage() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return statistics_.memory_usage_bytes.load();
    }

    double PlanCache::get_capacity_utilization() const
    {
        std::shared_lock<std::shared_mutex> config_lock(config_mutex_);
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);

        auto current_plans = statistics_.total_plans.load();
        auto max_plans = config_.max_plans;

        return static_cast<double>(current_plans) / max_plans;
    }

    void PlanCache::garbage_collect()
    {
        cleanup_expired_plans();

        if (should_evict()) {
            evict_plans(static_cast<std::uint32_t>(config_.max_plans * 0.1)); // Evict 10% of max
        }
    }

    std::string PlanCache::generate_performance_report() const
    {
        std::ostringstream report;

        std::shared_lock<std::shared_mutex> config_lock(config_mutex_);
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);

        report << "Plan Cache Performance Report\n";
        report << "============================\n\n";

        // Cache statistics
        report << "Cache Statistics:\n";
        report << "Total Plans: " << statistics_.total_plans.load() << "\n";
        report << "Generic Plans: " << statistics_.generic_plans.load() << "\n";
        report << "Specific Plans: " << statistics_.specific_plans.load() << "\n";
        report << "Prepared Plans: " << statistics_.prepared_plans.load() << "\n";
        report << "Hit Ratio: " << (statistics_.get_hit_ratio() * 100.0) << "%\n";
        report << "Cache Hits: " << statistics_.cache_hits.load() << "\n";
        report << "Cache Misses: " << statistics_.cache_misses.load() << "\n";
        report << "Cache Evictions: " << statistics_.cache_evictions.load() << "\n";
        report << "Cache Invalidations: " << statistics_.cache_invalidations.load() << "\n\n";

        // Memory usage
        report << "Memory Usage:\n";
        report << "Current Usage: " << (statistics_.memory_usage_bytes.load() / 1024 / 1024)
               << " MB\n";
        report << "Peak Usage: " << (statistics_.peak_memory_usage_bytes.load() / 1024 / 1024)
               << " MB\n";
        report << "Max Allowed: " << (config_.max_memory_bytes / 1024 / 1024) << " MB\n";
        report << "Utilization: " << (get_capacity_utilization() * 100.0) << "%\n\n";

        // Configuration
        report << "Configuration:\n";
        report << "Enabled: " << (config_.enabled ? "Yes" : "No") << "\n";
        report << "Max Plans: " << config_.max_plans << "\n";
        report << "Entry TTL: " << config_.entry_ttl_seconds << " seconds\n";
        report << "Cleanup Interval: " << config_.cleanup_interval_seconds << " seconds\n";

        return report.str();
    }

    // Private methods implementation

    void PlanCache::lru_add_to_head(std::shared_ptr<LRUNode> node)
    {
        node->prev = lru_head_;
        node->next = lru_head_->next;
        lru_head_->next->prev = node;
        lru_head_->next = node;
    }

    void PlanCache::lru_remove_node(std::shared_ptr<LRUNode> node)
    {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    void PlanCache::lru_move_to_head(std::shared_ptr<LRUNode> node)
    {
        lru_remove_node(node);
        lru_add_to_head(node);
    }

    std::shared_ptr<PlanCache::LRUNode> PlanCache::lru_remove_tail()
    {
        auto last_node = lru_tail_->prev;
        if (last_node == lru_head_) {
            return nullptr;
        }

        lru_remove_node(last_node);
        return last_node;
    }

    bool PlanCache::should_evict() const
    {
        std::shared_lock<std::shared_mutex> config_lock(config_mutex_);
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);

        auto current_plans = statistics_.total_plans.load();
        auto current_memory = statistics_.memory_usage_bytes.load();

        return current_plans >= config_.max_plans || current_memory >= config_.max_memory_bytes ||
               (static_cast<double>(current_plans) / config_.max_plans) >=
                   config_.eviction_threshold;
    }

    void PlanCache::update_memory_usage()
    {
        std::uint64_t total_memory = 0;

        std::shared_lock<std::shared_mutex> cache_lock(cache_mutex_);
        for (const auto& [hash, cached_plan] : cache_) {
            total_memory += cached_plan->get_memory_footprint();
        }

        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        statistics_.memory_usage_bytes.store(total_memory);

        auto peak = statistics_.peak_memory_usage_bytes.load();
        while (total_memory > peak &&
               !statistics_.peak_memory_usage_bytes.compare_exchange_weak(peak, total_memory)) {
            // Loop until we successfully update or find a higher value
        }
    }

    void PlanCache::enforce_memory_limits()
    {
        while (get_memory_usage() > config_.max_memory_bytes) {
            if (evict_plans(10) == 0) {
                break; // No more plans to evict
            }
        }
    }

    void PlanCache::cleanup_thread_main()
    {
        while (!shutdown_requested_.load()) {
            cleanup_expired_plans();
            garbage_collect();

            std::this_thread::sleep_for(std::chrono::seconds(config_.cleanup_interval_seconds));
        }
    }

    void PlanCache::update_hit_statistics()
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.cache_hits.fetch_add(1);
    }

    void PlanCache::update_miss_statistics()
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.cache_misses.fetch_add(1);
    }

    void PlanCache::update_eviction_statistics()
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.cache_evictions.fetch_add(1);
    }

    void PlanCache::update_insertion_statistics(std::chrono::microseconds duration)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_.total_insertion_time_us.fetch_add(duration.count());
    }

} // namespace scratchbird::engine

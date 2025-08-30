// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    // Forward declarations
    class QueryPlan;
    class ExecutionNode;
    class PlanNode;

    /// Execution statistics for cached plans
    struct ExecutionStatistics {
        /// Number of times this plan has been executed
        std::atomic<std::uint64_t> execution_count{0};

        /// Total execution time in microseconds
        std::atomic<std::uint64_t> total_execution_time_us{0};

        /// Average execution time in microseconds
        std::atomic<std::uint64_t> avg_execution_time_us{0};

        /// Minimum execution time in microseconds
        std::atomic<std::uint64_t> min_execution_time_us{UINT64_MAX};

        /// Maximum execution time in microseconds
        std::atomic<std::uint64_t> max_execution_time_us{0};

        /// Number of rows processed
        std::atomic<std::uint64_t> total_rows_processed{0};

        /// Number of times plan was invalidated
        std::atomic<std::uint32_t> invalidation_count{0};

        /// Last execution timestamp
        std::atomic<std::uint64_t> last_execution_time{0};

        /// Success ratio (0.0 to 1.0)
        std::atomic<double> success_ratio{1.0};

        /// Update execution statistics
        void update_execution_stats(std::uint64_t execution_time_us, std::uint64_t rows_processed,
                                    bool success = true);

        /// Reset statistics
        void reset();

        /// Get average execution time
        double get_avg_execution_time_us() const
        {
            auto count = execution_count.load();
            if (count == 0)
                return 0.0;
            return static_cast<double>(total_execution_time_us.load()) / count;
        }
    };

    /// Plan cache key for identifying cached plans
    struct PlanKey {
        /// SQL query text (normalized)
        std::string query_text;

        /// Database/schema context
        std::string database_name;
        std::string schema_name;

        /// Parameter type information
        std::vector<std::string> parameter_types;

        /// Query context flags
        bool read_only{false};
        bool transaction_context{false};
        std::uint32_t isolation_level{0};

        /// User context
        std::string user_name;
        std::vector<std::string> user_roles;

        /// Hash code for fast lookup
        mutable std::size_t hash_code{0};

        /// Default constructor
        PlanKey() = default;

        /// Constructor with query text
        explicit PlanKey(const std::string& query);

        /// Full constructor
        PlanKey(const std::string& query, const std::string& db_name,
                const std::string& schema_name);

        /// Equality comparison
        bool operator==(const PlanKey& other) const;

        /// Hash function
        std::size_t hash() const;

        /// String representation for debugging
        std::string to_string() const;

        /// Normalize query text (remove whitespace, lowercase keywords, etc.)
        static std::string normalize_query(const std::string& query);
    };

    /// Plan cache entry types
    enum class CachedPlanType {
        GENERIC_PLAN,      ///< Generic plan that works for any parameters
        SPECIFIC_PLAN,     ///< Plan optimized for specific parameter values
        PREPARED_PLAN,     ///< Prepared statement plan
        PARAMETERIZED_PLAN ///< Plan with parameter placeholders
    };

    /// Plan cache entry flags
    enum class CachedPlanFlags : std::uint32_t {
        NONE = 0x0,
        PINNED = 0x1,               ///< Plan cannot be evicted
        READ_ONLY = 0x2,            ///< Plan is for read-only queries
        EXPENSIVE = 0x4,            ///< Plan is expensive to recompute
        FREQUENTLY_USED = 0x8,      ///< Plan is accessed frequently
        RECENTLY_INVALIDATED = 0x10 ///< Plan was recently invalidated
    };

    /// Cached query plan
    class CachedPlan
    {
      public:
        /// Constructor
        CachedPlan(const PlanKey& key, std::shared_ptr<QueryPlan> plan,
                   CachedPlanType type = CachedPlanType::GENERIC_PLAN);

        /// Destructor
        ~CachedPlan() = default;

        /// Non-copyable, moveable
        CachedPlan(const CachedPlan&) = delete;
        CachedPlan& operator=(const CachedPlan&) = delete;
        CachedPlan(CachedPlan&&) = default;
        CachedPlan& operator=(CachedPlan&&) = default;

        /// Accessors
        const PlanKey& get_key() const
        {
            return key_;
        }
        std::shared_ptr<QueryPlan> get_plan() const
        {
            return plan_;
        }
        CachedPlanType get_type() const
        {
            return type_;
        }
        ExecutionStatistics& get_statistics()
        {
            return statistics_;
        }
        const ExecutionStatistics& get_statistics() const
        {
            return statistics_;
        }

        /// Plan metadata
        std::chrono::system_clock::time_point get_creation_time() const
        {
            return creation_time_;
        }
        std::chrono::system_clock::time_point get_last_access_time() const
        {
            return last_access_time_.load();
        }
        std::size_t get_plan_size_bytes() const
        {
            return plan_size_bytes_;
        }
        std::uint32_t get_access_count() const
        {
            return access_count_.load();
        }

        /// Flags management
        void set_flags(CachedPlanFlags flags)
        {
            flags_.store(static_cast<std::uint32_t>(flags));
        }
        void add_flags(CachedPlanFlags flags)
        {
            auto current = flags_.load();
            flags_.store(current | static_cast<std::uint32_t>(flags));
        }
        void remove_flags(CachedPlanFlags flags)
        {
            auto current = flags_.load();
            flags_.store(current & ~static_cast<std::uint32_t>(flags));
        }
        bool has_flags(CachedPlanFlags flags) const
        {
            return (flags_.load() & static_cast<std::uint32_t>(flags)) != 0;
        }

        /// Access tracking
        void record_access();

        /// Plan validation
        bool is_valid() const
        {
            return plan_ != nullptr && is_valid_.load();
        }
        void invalidate()
        {
            is_valid_.store(false);
        }

        /// Plan cost estimation
        double get_estimated_cost() const
        {
            return estimated_cost_;
        }
        void set_estimated_cost(double cost)
        {
            estimated_cost_ = cost;
        }

        /// Memory footprint
        std::size_t get_memory_footprint() const;

      private:
        /// Plan identification and data
        PlanKey key_;
        std::shared_ptr<QueryPlan> plan_;
        CachedPlanType type_;

        /// Execution statistics
        ExecutionStatistics statistics_;

        /// Cache metadata
        std::chrono::system_clock::time_point creation_time_;
        std::atomic<std::chrono::system_clock::time_point> last_access_time_;
        std::size_t plan_size_bytes_;
        std::atomic<std::uint32_t> access_count_{0};
        std::atomic<std::uint32_t> flags_{0};
        std::atomic<bool> is_valid_{true};

        /// Cost information
        double estimated_cost_{0.0};

        /// Calculate plan size in bytes
        std::size_t calculate_plan_size() const;
    };

    /// Plan cache configuration
    struct PlanCacheConfig {
        /// Maximum number of cached plans
        std::uint32_t max_plans{1000};

        /// Maximum memory usage in bytes
        std::uint64_t max_memory_bytes{64 * 1024 * 1024}; // 64 MB

        /// Cache entry TTL in seconds
        std::uint32_t entry_ttl_seconds{3600}; // 1 hour

        /// LRU eviction threshold (0.0-1.0)
        double eviction_threshold{0.8};

        /// Enable/disable plan caching
        bool enabled{true};

        /// Enable/disable generic plan caching
        bool enable_generic_plans{true};

        /// Enable/disable parameter-specific plans
        bool enable_specific_plans{true};

        /// Minimum executions before creating specific plan
        std::uint32_t specific_plan_threshold{5};

        /// Plan cache warming on startup
        bool enable_cache_warming{true};

        /// Background cleanup interval in seconds
        std::uint32_t cleanup_interval_seconds{300}; // 5 minutes

        /// Enable plan cache statistics
        bool enable_statistics{true};

        /// Validation
        bool is_valid() const;
        std::string validate() const;
    };

    /// Plan cache statistics
    struct PlanCacheStats {
        /// Cache hit/miss statistics
        std::atomic<std::uint64_t> cache_hits{0};
        std::atomic<std::uint64_t> cache_misses{0};
        std::atomic<std::uint64_t> cache_evictions{0};
        std::atomic<std::uint64_t> cache_invalidations{0};

        /// Plan statistics
        std::atomic<std::uint32_t> total_plans{0};
        std::atomic<std::uint32_t> generic_plans{0};
        std::atomic<std::uint32_t> specific_plans{0};
        std::atomic<std::uint32_t> prepared_plans{0};

        /// Memory usage
        std::atomic<std::uint64_t> memory_usage_bytes{0};
        std::atomic<std::uint64_t> peak_memory_usage_bytes{0};

        /// Performance metrics
        std::atomic<std::uint64_t> total_lookup_time_us{0};
        std::atomic<std::uint64_t> total_insertion_time_us{0};
        std::atomic<std::uint64_t> total_eviction_time_us{0};

        /// Plan age statistics
        std::atomic<std::uint64_t> avg_plan_age_seconds{0};
        std::atomic<std::uint64_t> oldest_plan_age_seconds{0};

        /// Default constructor
        PlanCacheStats() = default;

        /// Copy constructor
        PlanCacheStats(const PlanCacheStats& other)
        {
            copy_from(other);
        }

        /// Copy assignment
        PlanCacheStats& operator=(const PlanCacheStats& other)
        {
            if (this != &other) {
                copy_from(other);
            }
            return *this;
        }

      private:
        /// Copy atomic values from another instance
        void copy_from(const PlanCacheStats& other)
        {
            cache_hits.store(other.cache_hits.load());
            cache_misses.store(other.cache_misses.load());
            cache_evictions.store(other.cache_evictions.load());
            cache_invalidations.store(other.cache_invalidations.load());
            total_plans.store(other.total_plans.load());
            generic_plans.store(other.generic_plans.load());
            specific_plans.store(other.specific_plans.load());
            prepared_plans.store(other.prepared_plans.load());
            memory_usage_bytes.store(other.memory_usage_bytes.load());
            peak_memory_usage_bytes.store(other.peak_memory_usage_bytes.load());
            total_lookup_time_us.store(other.total_lookup_time_us.load());
            total_insertion_time_us.store(other.total_insertion_time_us.load());
            total_eviction_time_us.store(other.total_eviction_time_us.load());
            avg_plan_age_seconds.store(other.avg_plan_age_seconds.load());
            oldest_plan_age_seconds.store(other.oldest_plan_age_seconds.load());
        }

      public:
        /// Hit ratio calculation
        double get_hit_ratio() const
        {
            auto hits = cache_hits.load();
            auto misses = cache_misses.load();
            if (hits + misses == 0)
                return 0.0;
            return static_cast<double>(hits) / (hits + misses);
        }

        /// Reset statistics
        void reset()
        {
            cache_hits.store(0);
            cache_misses.store(0);
            cache_evictions.store(0);
            cache_invalidations.store(0);
            total_plans.store(0);
            generic_plans.store(0);
            specific_plans.store(0);
            prepared_plans.store(0);
            memory_usage_bytes.store(0);
            peak_memory_usage_bytes.store(0);
            total_lookup_time_us.store(0);
            total_insertion_time_us.store(0);
            total_eviction_time_us.store(0);
            avg_plan_age_seconds.store(0);
            oldest_plan_age_seconds.store(0);
        }
    };

    /// Query Plan Cache
    class PlanCache
    {
      public:
        /// Constructor
        explicit PlanCache(const PlanCacheConfig& config = PlanCacheConfig{});

        /// Destructor
        ~PlanCache();

        /// Non-copyable, non-moveable
        PlanCache(const PlanCache&) = delete;
        PlanCache& operator=(const PlanCache&) = delete;
        PlanCache(PlanCache&&) = delete;
        PlanCache& operator=(PlanCache&&) = delete;

        /// Initialize plan cache
        bool initialize();

        /// Shutdown plan cache
        void shutdown();

        /// Plan cache operations

        /// Insert a plan into the cache
        bool insert_plan(const PlanKey& key, std::shared_ptr<QueryPlan> plan,
                         CachedPlanType type = CachedPlanType::GENERIC_PLAN);

        /// Lookup a plan in the cache
        std::shared_ptr<CachedPlan> lookup_plan(const PlanKey& key);

        /// Remove a plan from the cache
        bool remove_plan(const PlanKey& key);

        /// Evict plans based on LRU policy
        std::uint32_t evict_plans(std::uint32_t max_evictions = UINT32_MAX);

        /// Clear all plans from the cache
        void clear();

        /// Plan validation and invalidation

        /// Invalidate plans by pattern (e.g., all plans for a table)
        std::uint32_t invalidate_plans(const std::string& pattern);

        /// Invalidate plans by database/schema
        std::uint32_t invalidate_plans_by_schema(const std::string& database_name,
                                                 const std::string& schema_name);

        /// Cleanup expired plans
        std::uint32_t cleanup_expired_plans();

        /// Configuration and statistics

        /// Update cache configuration
        bool update_config(const PlanCacheConfig& config);

        /// Get current configuration
        PlanCacheConfig get_config() const;

        /// Get cache statistics
        PlanCacheStats get_statistics() const;

        /// Reset cache statistics
        void reset_statistics();

        /// Cache introspection

        /// Get all cached plan keys
        std::vector<PlanKey> get_all_plan_keys() const;

        /// Get plan details by key
        std::shared_ptr<CachedPlan> get_plan_details(const PlanKey& key) const;

        /// Get plans by type
        std::vector<std::shared_ptr<CachedPlan>> get_plans_by_type(CachedPlanType type) const;

        /// Memory management

        /// Get current memory usage
        std::uint64_t get_memory_usage() const;

        /// Get cache capacity utilization (0.0 to 1.0)
        double get_capacity_utilization() const;

        /// Trigger garbage collection
        void garbage_collect();

        /// Plan cache warming

        /// Warm cache from query log
        std::uint32_t warm_cache_from_log(const std::string& log_file_path);

        /// Pre-compile frequent queries
        std::uint32_t precompile_frequent_queries();

        /// Performance monitoring

        /// Enable/disable performance monitoring
        void set_monitoring_enabled(bool enabled);

        /// Get performance report
        std::string generate_performance_report() const;

        /// Export cache statistics
        bool export_statistics(const std::string& file_path) const;

      private:
        /// Configuration
        PlanCacheConfig config_;
        mutable std::shared_mutex config_mutex_;

        /// Cache storage
        std::unordered_map<std::size_t, std::shared_ptr<CachedPlan>> cache_;
        mutable std::shared_mutex cache_mutex_;

        /// LRU tracking
        struct LRUNode {
            std::size_t key_hash;
            std::shared_ptr<CachedPlan> plan;
            std::shared_ptr<LRUNode> prev;
            std::shared_ptr<LRUNode> next;

            LRUNode(std::size_t hash, std::shared_ptr<CachedPlan> p) : key_hash(hash), plan(p) {}
        };

        std::shared_ptr<LRUNode> lru_head_;
        std::shared_ptr<LRUNode> lru_tail_;
        std::unordered_map<std::size_t, std::shared_ptr<LRUNode>> lru_map_;
        mutable std::mutex lru_mutex_;

        /// Statistics
        PlanCacheStats statistics_;
        mutable std::mutex stats_mutex_;

        /// Background cleanup thread
        std::unique_ptr<std::thread> cleanup_thread_;
        std::atomic<bool> shutdown_requested_{false};

        /// Private methods

        /// LRU operations
        void lru_add_to_head(std::shared_ptr<LRUNode> node);
        void lru_remove_node(std::shared_ptr<LRUNode> node);
        void lru_move_to_head(std::shared_ptr<LRUNode> node);
        std::shared_ptr<LRUNode> lru_remove_tail();

        /// Eviction policy
        bool should_evict() const;
        std::vector<std::shared_ptr<CachedPlan>> select_eviction_candidates() const;

        /// Memory management
        void update_memory_usage();
        void enforce_memory_limits();

        /// Background cleanup
        void cleanup_thread_main();

        /// Statistics updates
        void update_hit_statistics();
        void update_miss_statistics();
        void update_eviction_statistics();
        void update_insertion_statistics(std::chrono::microseconds duration);
    };

} // namespace scratchbird::engine

/// Hash function for PlanKey
template <> struct std::hash<scratchbird::engine::PlanKey> {
    std::size_t operator()(const scratchbird::engine::PlanKey& key) const
    {
        return key.hash();
    }
};

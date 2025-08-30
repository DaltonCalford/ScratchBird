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
    class PreparedStatement;

    /// Parameter descriptor for prepared statements
    struct ParameterDescriptor {
        /// Parameter name (if named parameter)
        std::string name;

        /// Parameter type
        std::string type_name;

        /// Parameter ordinal position (1-based)
        std::uint32_t ordinal{0};

        /// Is parameter required
        bool required{true};

        /// Default value (if any)
        std::string default_value;

        /// Parameter mode (IN, OUT, INOUT)
        enum class Mode { IN, OUT, INOUT } mode{Mode::IN};

        /// Construct parameter descriptor
        ParameterDescriptor() = default;
        ParameterDescriptor(const std::string& param_name, const std::string& type,
                            std::uint32_t pos)
            : name(param_name), type_name(type), ordinal(pos)
        {
        }
    };

    /// Column metadata for result sets
    struct ColumnMetadata {
        /// Column name
        std::string name;

        /// Column type
        std::string type_name;

        /// Column ordinal position (0-based)
        std::uint32_t ordinal{0};

        /// Maximum column width (-1 for unlimited)
        std::int32_t max_width{-1};

        /// Is column nullable
        bool nullable{true};

        /// Column precision (for numeric types)
        std::uint32_t precision{0};

        /// Column scale (for numeric types)
        std::uint32_t scale{0};

        /// Construct column metadata
        ColumnMetadata() = default;
        ColumnMetadata(const std::string& col_name, const std::string& type, std::uint32_t pos)
            : name(col_name), type_name(type), ordinal(pos)
        {
        }
    };

    /// Statement metadata combining parameters and result columns
    struct StatementMetadata {
        /// Statement type (SELECT, INSERT, UPDATE, DELETE, etc.)
        std::string statement_type;

        /// Input parameters
        std::vector<ParameterDescriptor> parameters;

        /// Result columns (empty for non-SELECT statements)
        std::vector<ColumnMetadata> columns;

        /// Estimated execution cost
        double estimated_cost{0.0};

        /// Estimated result row count
        std::uint64_t estimated_rows{0};

        /// Is statement read-only
        bool read_only{false};

        /// Does statement return results
        bool returns_results{false};

        /// Statement complexity level
        enum class Complexity { SIMPLE, MODERATE, COMPLEX } complexity{Complexity::SIMPLE};

        /// Creation timestamp
        std::chrono::system_clock::time_point creation_time{std::chrono::system_clock::now()};

        /// Default constructor
        StatementMetadata() = default;

        /// Constructor with statement type
        explicit StatementMetadata(const std::string& stmt_type) : statement_type(stmt_type) {}
    };

    /// Prepared statement cache key for identifying cached statements
    struct PreparedStatementKey {
        /// SQL statement text (normalized)
        std::string sql_text;

        /// Database/schema context
        std::string database_name;
        std::string schema_name;

        /// User context
        std::string user_name;
        std::vector<std::string> user_roles;

        /// Statement options
        bool case_sensitive{false};
        std::uint32_t timeout_seconds{0};

        /// Hash code for fast lookup
        mutable std::size_t hash_code{0};

        /// Default constructor
        PreparedStatementKey() = default;

        /// Constructor with SQL text
        explicit PreparedStatementKey(const std::string& sql) : sql_text(normalize_sql(sql)) {}

        /// Full constructor
        PreparedStatementKey(const std::string& sql, const std::string& db_name,
                             const std::string& schema_name, const std::string& user_name)
            : sql_text(normalize_sql(sql)), database_name(db_name), schema_name(schema_name),
              user_name(user_name)
        {
        }

        /// Equality comparison
        bool operator==(const PreparedStatementKey& other) const;

        /// Inequality comparison
        bool operator!=(const PreparedStatementKey& other) const
        {
            return !(*this == other);
        }

        /// Hash function
        std::size_t hash() const;

        /// String representation for debugging
        std::string to_string() const;

        /// Normalize SQL text (remove extra whitespace, standardize keywords)
        static std::string normalize_sql(const std::string& sql);
    };

    /// Cached prepared statement execution statistics
    struct PreparedStatementStats {
        /// Number of times this statement has been executed
        std::atomic<std::uint64_t> execution_count{0};

        /// Total preparation time in microseconds
        std::atomic<std::uint64_t> total_preparation_time_us{0};

        /// Average preparation time in microseconds
        std::atomic<std::uint64_t> avg_preparation_time_us{0};

        /// Total execution time in microseconds
        std::atomic<std::uint64_t> total_execution_time_us{0};

        /// Average execution time in microseconds
        std::atomic<std::uint64_t> avg_execution_time_us{0};

        /// Number of rows processed
        std::atomic<std::uint64_t> total_rows_processed{0};

        /// Last execution timestamp
        std::atomic<std::uint64_t> last_execution_time{0};

        /// Success ratio (0.0 to 1.0)
        std::atomic<double> success_ratio{1.0};

        /// Cache hit count for this statement
        std::atomic<std::uint64_t> cache_hit_count{0};

        /// Default constructor
        PreparedStatementStats() = default;

        /// Copy constructor
        PreparedStatementStats(const PreparedStatementStats& other)
        {
            copy_from(other);
        }

        /// Copy assignment
        PreparedStatementStats& operator=(const PreparedStatementStats& other)
        {
            if (this != &other) {
                copy_from(other);
            }
            return *this;
        }

        /// Update execution statistics
        void update_execution_stats(std::uint64_t execution_time_us, std::uint64_t rows_processed,
                                    bool success = true);

        /// Update preparation statistics
        void update_preparation_stats(std::uint64_t preparation_time_us);

        /// Record cache hit
        void record_cache_hit();

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

        /// Get average preparation time
        double get_avg_preparation_time_us() const
        {
            auto count = execution_count.load();
            if (count == 0)
                return 0.0;
            return static_cast<double>(total_preparation_time_us.load()) / count;
        }

      private:
        /// Copy atomic values from another instance
        void copy_from(const PreparedStatementStats& other)
        {
            execution_count.store(other.execution_count.load());
            total_preparation_time_us.store(other.total_preparation_time_us.load());
            avg_preparation_time_us.store(other.avg_preparation_time_us.load());
            total_execution_time_us.store(other.total_execution_time_us.load());
            avg_execution_time_us.store(other.avg_execution_time_us.load());
            total_rows_processed.store(other.total_rows_processed.load());
            last_execution_time.store(other.last_execution_time.load());
            success_ratio.store(other.success_ratio.load());
            cache_hit_count.store(other.cache_hit_count.load());
        }
    };

    /// Cached prepared statement
    class CachedPreparedStatement
    {
      public:
        /// Constructor
        CachedPreparedStatement(const PreparedStatementKey& key,
                                std::shared_ptr<PreparedStatement> statement,
                                const StatementMetadata& metadata);

        /// Destructor
        ~CachedPreparedStatement() = default;

        /// Non-copyable, moveable
        CachedPreparedStatement(const CachedPreparedStatement&) = delete;
        CachedPreparedStatement& operator=(const CachedPreparedStatement&) = delete;
        CachedPreparedStatement(CachedPreparedStatement&&) = default;
        CachedPreparedStatement& operator=(CachedPreparedStatement&&) = default;

        /// Accessors
        const PreparedStatementKey& get_key() const
        {
            return key_;
        }
        std::shared_ptr<PreparedStatement> get_statement() const
        {
            return statement_;
        }
        const StatementMetadata& get_metadata() const
        {
            return metadata_;
        }
        PreparedStatementStats& get_statistics()
        {
            return statistics_;
        }
        const PreparedStatementStats& get_statistics() const
        {
            return statistics_;
        }

        /// Statement metadata
        std::chrono::system_clock::time_point get_creation_time() const
        {
            return creation_time_;
        }
        std::chrono::system_clock::time_point get_last_access_time() const
        {
            return last_access_time_.load();
        }
        std::size_t get_statement_size_bytes() const
        {
            return statement_size_bytes_;
        }
        std::uint32_t get_access_count() const
        {
            return access_count_.load();
        }

        /// Access tracking
        void record_access();

        /// Statement validation
        bool is_valid() const
        {
            return statement_ != nullptr && is_valid_.load();
        }
        void invalidate()
        {
            is_valid_.store(false);
        }

        /// Memory footprint
        std::size_t get_memory_footprint() const;

      private:
        /// Statement identification and data
        PreparedStatementKey key_;
        std::shared_ptr<PreparedStatement> statement_;
        StatementMetadata metadata_;

        /// Execution statistics
        PreparedStatementStats statistics_;

        /// Cache metadata
        std::chrono::system_clock::time_point creation_time_;
        std::atomic<std::chrono::system_clock::time_point> last_access_time_;
        std::size_t statement_size_bytes_;
        std::atomic<std::uint32_t> access_count_{0};
        std::atomic<bool> is_valid_{true};

        /// Calculate statement size in bytes
        std::size_t calculate_statement_size() const;
    };

    /// Prepared statement cache configuration
    struct PreparedStatementCacheConfig {
        /// Maximum number of cached prepared statements
        std::uint32_t max_statements{500};

        /// Maximum memory usage in bytes
        std::uint64_t max_memory_bytes{32 * 1024 * 1024}; // 32 MB

        /// Cache entry TTL in seconds
        std::uint32_t entry_ttl_seconds{1800}; // 30 minutes

        /// LRU eviction threshold (0.0-1.0)
        double eviction_threshold{0.75};

        /// Enable/disable prepared statement caching
        bool enabled{true};

        /// Enable/disable metadata caching
        bool enable_metadata_cache{true};

        /// Minimum executions before caching
        std::uint32_t cache_threshold{2};

        /// Background cleanup interval in seconds
        std::uint32_t cleanup_interval_seconds{180}; // 3 minutes

        /// Enable prepared statement cache statistics
        bool enable_statistics{true};

        /// Validation
        bool is_valid() const;
        std::string validate() const;
    };

    /// Prepared statement cache statistics
    struct PreparedStatementCacheStats {
        /// Cache hit/miss statistics
        std::atomic<std::uint64_t> cache_hits{0};
        std::atomic<std::uint64_t> cache_misses{0};
        std::atomic<std::uint64_t> cache_evictions{0};
        std::atomic<std::uint64_t> cache_invalidations{0};

        /// Statement statistics
        std::atomic<std::uint32_t> total_statements{0};
        std::atomic<std::uint32_t> active_statements{0};
        std::atomic<std::uint32_t> expired_statements{0};

        /// Memory usage
        std::atomic<std::uint64_t> memory_usage_bytes{0};
        std::atomic<std::uint64_t> peak_memory_usage_bytes{0};

        /// Performance metrics
        std::atomic<std::uint64_t> total_lookup_time_us{0};
        std::atomic<std::uint64_t> total_insertion_time_us{0};
        std::atomic<std::uint64_t> total_eviction_time_us{0};

        /// Preparation time savings
        std::atomic<std::uint64_t> preparation_time_saved_us{0};

        /// Default constructor
        PreparedStatementCacheStats() = default;

        /// Copy constructor
        PreparedStatementCacheStats(const PreparedStatementCacheStats& other)
        {
            copy_from(other);
        }

        /// Copy assignment
        PreparedStatementCacheStats& operator=(const PreparedStatementCacheStats& other)
        {
            if (this != &other) {
                copy_from(other);
            }
            return *this;
        }

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
            total_statements.store(0);
            active_statements.store(0);
            expired_statements.store(0);
            memory_usage_bytes.store(0);
            peak_memory_usage_bytes.store(0);
            total_lookup_time_us.store(0);
            total_insertion_time_us.store(0);
            total_eviction_time_us.store(0);
            preparation_time_saved_us.store(0);
        }

      private:
        /// Copy atomic values from another instance
        void copy_from(const PreparedStatementCacheStats& other)
        {
            cache_hits.store(other.cache_hits.load());
            cache_misses.store(other.cache_misses.load());
            cache_evictions.store(other.cache_evictions.load());
            cache_invalidations.store(other.cache_invalidations.load());
            total_statements.store(other.total_statements.load());
            active_statements.store(other.active_statements.load());
            expired_statements.store(other.expired_statements.load());
            memory_usage_bytes.store(other.memory_usage_bytes.load());
            peak_memory_usage_bytes.store(other.peak_memory_usage_bytes.load());
            total_lookup_time_us.store(other.total_lookup_time_us.load());
            total_insertion_time_us.store(other.total_insertion_time_us.load());
            total_eviction_time_us.store(other.total_eviction_time_us.load());
            preparation_time_saved_us.store(other.preparation_time_saved_us.load());
        }
    };

    /// Prepared Statement Cache
    class PreparedStatementCache
    {
      public:
        /// Constructor
        explicit PreparedStatementCache(
            const PreparedStatementCacheConfig& config = PreparedStatementCacheConfig{});

        /// Destructor
        ~PreparedStatementCache();

        /// Non-copyable, non-moveable
        PreparedStatementCache(const PreparedStatementCache&) = delete;
        PreparedStatementCache& operator=(const PreparedStatementCache&) = delete;
        PreparedStatementCache(PreparedStatementCache&&) = delete;
        PreparedStatementCache& operator=(PreparedStatementCache&&) = delete;

        /// Initialize prepared statement cache
        bool initialize();

        /// Shutdown prepared statement cache
        void shutdown();

        /// Cache operations

        /// Insert a prepared statement into the cache
        bool insert_statement(const PreparedStatementKey& key,
                              std::shared_ptr<PreparedStatement> statement,
                              const StatementMetadata& metadata);

        /// Lookup a prepared statement in the cache
        std::shared_ptr<CachedPreparedStatement> lookup_statement(const PreparedStatementKey& key);

        /// Remove a prepared statement from the cache
        bool remove_statement(const PreparedStatementKey& key);

        /// Evict statements based on LRU policy
        std::uint32_t evict_statements(std::uint32_t max_evictions = UINT32_MAX);

        /// Clear all statements from the cache
        void clear();

        /// Statement validation and invalidation

        /// Invalidate statements by pattern (e.g., all statements for a table)
        std::uint32_t invalidate_statements(const std::string& pattern);

        /// Invalidate statements by database/schema
        std::uint32_t invalidate_statements_by_schema(const std::string& database_name,
                                                      const std::string& schema_name);

        /// Cleanup expired statements
        std::uint32_t cleanup_expired_statements();

        /// Configuration and statistics

        /// Update cache configuration
        bool update_config(const PreparedStatementCacheConfig& config);

        /// Get current configuration
        PreparedStatementCacheConfig get_config() const;

        /// Get cache statistics
        PreparedStatementCacheStats get_statistics() const;

        /// Reset cache statistics
        void reset_statistics();

        /// Cache introspection

        /// Get all cached statement keys
        std::vector<PreparedStatementKey> get_all_statement_keys() const;

        /// Get statement details by key
        std::shared_ptr<CachedPreparedStatement>
        get_statement_details(const PreparedStatementKey& key) const;

        /// Memory management

        /// Get current memory usage
        std::uint64_t get_memory_usage() const;

        /// Get cache capacity utilization (0.0 to 1.0)
        double get_capacity_utilization() const;

        /// Trigger garbage collection
        void garbage_collect();

        /// Performance monitoring

        /// Enable/disable performance monitoring
        void set_monitoring_enabled(bool enabled);

        /// Get performance report
        std::string generate_performance_report() const;

        /// Export cache statistics
        bool export_statistics(const std::string& file_path) const;

      private:
        /// Configuration
        PreparedStatementCacheConfig config_;
        mutable std::shared_mutex config_mutex_;

        /// Cache storage
        std::unordered_map<std::size_t, std::shared_ptr<CachedPreparedStatement>> cache_;
        mutable std::shared_mutex cache_mutex_;

        /// LRU tracking
        struct LRUNode {
            std::size_t key_hash;
            std::shared_ptr<CachedPreparedStatement> statement;
            std::shared_ptr<LRUNode> prev;
            std::shared_ptr<LRUNode> next;

            LRUNode(std::size_t hash, std::shared_ptr<CachedPreparedStatement> stmt)
                : key_hash(hash), statement(stmt)
            {
            }
        };

        std::shared_ptr<LRUNode> lru_head_;
        std::shared_ptr<LRUNode> lru_tail_;
        std::unordered_map<std::size_t, std::shared_ptr<LRUNode>> lru_map_;
        mutable std::mutex lru_mutex_;

        /// Statistics
        PreparedStatementCacheStats statistics_;
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
        std::vector<std::shared_ptr<CachedPreparedStatement>> select_eviction_candidates() const;

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

/// Hash function for PreparedStatementKey
template <> struct std::hash<scratchbird::engine::PreparedStatementKey> {
    std::size_t operator()(const scratchbird::engine::PreparedStatementKey& key) const
    {
        return key.hash();
    }
};

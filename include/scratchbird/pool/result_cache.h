#pragma once

/**
 * @file result_cache.h
 * @brief Result Set Cache for Connection Pooling
 *
 * This file defines the result set caching system that stores query
 * results for reuse, providing significant performance improvements
 * for repeated queries.
 *
 * Features:
 * - Query-based and table-based caching
 * - Automatic invalidation on data changes
 * - Memory-bounded with configurable eviction
 * - TTL-based expiration
 * - Partial result caching for large result sets
 *
 * Part of Phase 3.6: Connection Pooling
 */

#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace scratchbird {
namespace pool {

// Forward declarations
class DatabaseResultCache;
class CachedResult;

/**
 * @brief Eviction policy for result cache
 */
enum class ResultEvictionPolicy {
    LRU,   // Least Recently Used (default)
    LFU,   // Least Frequently Used
    SIZE,  // Largest result sets first
    TTL    // Shortest TTL first
};

/**
 * @brief Cache entry state
 */
enum class ResultCacheEntryState {
    VALID,
    LOADING,
    PARTIAL,
    STALE,
    INVALID
};

/**
 * @brief Configuration for result cache
 */
struct ResultCacheConfig {
    // Size limits
    uint64_t max_memory_bytes = 256 * 1024 * 1024;  // 256MB default
    uint32_t max_entries = 10000;                    // Maximum cached results
    uint64_t max_result_size = 16 * 1024 * 1024;     // Max 16MB per result
    uint32_t max_rows_per_result = 100000;           // Max rows per result

    // Eviction policy
    ResultEvictionPolicy eviction_policy = ResultEvictionPolicy::LRU;

    // TTL settings
    std::chrono::seconds default_ttl{300};     // 5 minutes default
    std::chrono::seconds min_ttl{10};          // Minimum TTL
    std::chrono::seconds max_ttl{3600};        // Maximum TTL (1 hour)
    bool honor_query_hints = true;             // Respect SQL hints for TTL

    // Invalidation settings
    bool invalidate_on_insert = true;
    bool invalidate_on_update = true;
    bool invalidate_on_delete = true;
    bool invalidate_on_truncate = true;
    bool invalidate_on_ddl = true;
    bool use_transaction_boundaries = true;  // Invalidate on commit only

    // Caching criteria
    uint32_t min_rows_to_cache = 0;       // Minimum rows (0 = cache everything)
    uint32_t max_rows_to_cache = 10000;   // Maximum rows to cache
    uint32_t max_rows_per_result = 100000; // Hard cap on cached rows
    uint64_t min_cost_to_cache = 0;       // Minimum query cost
    bool cache_empty_results = true;       // Cache zero-row results
    bool require_deterministic = true;     // Only cache deterministic queries
    bool require_snapshot_safe = true;     // Only cache snapshot-stable queries

    // Partial caching
    bool enable_partial_caching = true;   // Cache partial results
    uint32_t partial_cache_rows = 1000;   // Rows to cache for partial

    // Query filtering
    bool cache_parameterized_only = false;  // Only cache with parameters
    std::vector<std::string> excluded_tables;  // Tables to never cache

    // Performance tuning
    uint32_t hash_bucket_count = 4096;
    bool enable_compression = false;
    uint32_t compression_threshold = 4096;  // Compress if > this size
};

/**
 * @brief Column information for cached results
 */
struct CachedColumnInfo {
    std::string name;
    uint32_t type_oid = 0;
    int32_t type_mod = -1;
    uint16_t format = 0;  // 0 = text, 1 = binary
    bool nullable = true;
};

/**
 * @brief Represents a value in cached result (variant type)
 */
using CachedValue = std::variant<
    std::monostate,           // NULL
    bool,                     // BOOLEAN
    int16_t,                  // SMALLINT
    int32_t,                  // INTEGER
    int64_t,                  // BIGINT
    float,                    // REAL
    double,                   // DOUBLE PRECISION
    std::string,              // VARCHAR, TEXT, CHAR, etc.
    std::vector<uint8_t>      // BYTEA, binary data
>;

/**
 * @brief A row in a cached result
 */
using CachedRow = std::vector<CachedValue>;

/**
 * @brief Metadata about a cached result
 */
struct ResultMetadata {
    // Query identification
    std::string sql;
    std::string fingerprint;
    uint64_t fingerprint_hash = 0;
    std::vector<CachedValue> parameters;  // Bound parameter values
    uint64_t schema_version_id = 0;
    std::string privilege_signature;

    // Table references (for invalidation)
    std::vector<std::string> referenced_tables;
    std::vector<std::string> referenced_schemas;

    // Result shape
    std::vector<CachedColumnInfo> columns;
    uint64_t row_count = 0;
    uint64_t total_rows = 0;  // May differ if partial

    // Storage
    uint64_t memory_bytes = 0;
    uint64_t uncompressed_bytes = 0;
    bool is_compressed = false;
    bool is_partial = false;

    // Query execution info
    std::chrono::microseconds execution_time{0};
    double query_cost = 0.0;
    bool is_deterministic = true;
    bool is_snapshot_safe = true;
    bool contains_volatile_functions = false;
};

/**
 * @brief Statistics for a cached result entry
 */
struct ResultEntryStats {
    // Usage counters
    uint64_t hit_count = 0;
    uint64_t read_count = 0;  // Total rows read

    // Timing
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_accessed;
    std::chrono::system_clock::time_point expires_at;

    // Performance impact
    std::chrono::microseconds time_saved{0};  // Estimated time saved by cache
};

/**
 * @brief A cached query result
 */
class CachedResult {
public:
    CachedResult() = default;
    CachedResult(const std::string& sql, const ResultMetadata& metadata);
    ~CachedResult() = default;

    // Non-copyable, movable
    CachedResult(const CachedResult&) = delete;
    CachedResult& operator=(const CachedResult&) = delete;
    CachedResult(CachedResult&&) noexcept = default;
    CachedResult& operator=(CachedResult&&) noexcept = default;

    // Identification
    const std::string& sql() const { return metadata_.sql; }
    const std::string& fingerprint() const { return metadata_.fingerprint; }
    uint64_t fingerprint_hash() const { return metadata_.fingerprint_hash; }

    // Metadata access
    const ResultMetadata& metadata() const { return metadata_; }
    ResultMetadata& metadata() { return metadata_; }

    // Statistics
    const ResultEntryStats& stats() const { return stats_; }
    ResultEntryStats& stats() { return stats_; }

    // State
    ResultCacheEntryState state() const { return state_; }
    void set_state(ResultCacheEntryState state) { state_ = state; }
    bool is_valid() const { return state_ == ResultCacheEntryState::VALID; }
    bool is_expired() const;
    bool is_partial() const { return metadata_.is_partial; }

    // Row access
    uint64_t row_count() const { return rows_.size(); }
    const CachedRow& row(uint64_t index) const { return rows_.at(index); }
    const std::vector<CachedRow>& rows() const { return rows_; }

    // Modify rows (during population)
    void add_row(CachedRow row);
    void set_rows(std::vector<CachedRow> rows);
    void clear_rows();

    // Column info
    uint32_t column_count() const { return metadata_.columns.size(); }
    const CachedColumnInfo& column(uint32_t index) const { return metadata_.columns.at(index); }
    const std::vector<CachedColumnInfo>& columns() const { return metadata_.columns; }

    // Usage tracking
    void record_hit();
    void record_rows_read(uint64_t count);

    // Memory
    uint64_t memory_usage() const { return metadata_.memory_bytes; }
    void update_memory_usage();

    // TTL
    void set_ttl(std::chrono::seconds ttl);
    std::chrono::seconds remaining_ttl() const;
    void refresh_ttl();

    // Compression (for large results)
    bool compress();
    bool decompress();

private:
    ResultMetadata metadata_;
    ResultEntryStats stats_;
    ResultCacheEntryState state_ = ResultCacheEntryState::VALID;
    std::vector<CachedRow> rows_;
    std::vector<uint8_t> compressed_data_;  // If compressed
    std::chrono::seconds ttl_{300};
};

/**
 * @brief Aggregate statistics for result cache
 */
struct ResultCacheStatistics {
    // Size metrics
    uint64_t entry_count = 0;
    uint64_t total_rows = 0;
    uint64_t memory_bytes = 0;

    // Hit/miss ratios
    uint64_t total_hits = 0;
    uint64_t total_misses = 0;
    double hit_ratio = 0.0;

    // Eviction metrics
    uint64_t eviction_count = 0;
    uint64_t invalidation_count = 0;
    uint64_t expiration_count = 0;

    // Performance impact
    std::chrono::microseconds total_time_saved{0};
    uint64_t total_rows_served = 0;
    uint64_t queries_served_from_cache = 0;

    // By table
    std::unordered_map<std::string, uint64_t> hits_by_table;
    std::unordered_map<std::string, uint64_t> invalidations_by_table;

    // Timestamps
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_eviction;
    std::chrono::system_clock::time_point last_invalidation;
};

/**
 * @brief Cache key generator for result cache
 *
 * Generates unique keys based on SQL + parameters
 */
class ResultCacheKeyGenerator {
public:
    ResultCacheKeyGenerator() = default;

    /**
     * @brief Generate cache key from SQL and parameters
     * @param sql The SQL statement
     * @param parameters Bound parameters
     * @return Cache key string
     */
    std::string generate_key(
        std::string_view sql,
        const std::vector<CachedValue>& parameters) const;
    std::string generate_key(
        std::string_view sql,
        const std::vector<CachedValue>& parameters,
        uint64_t schema_version_id,
        const std::string& privilege_signature) const;

    /**
     * @brief Generate cache key hash
     * @param key The cache key
     * @return 64-bit hash
     */
    uint64_t hash(std::string_view key) const;

    /**
     * @brief Extract tables from SQL for invalidation tracking
     * @param sql The SQL statement
     * @return List of table names
     */
    std::vector<std::string> extract_tables(std::string_view sql) const;

private:
    std::string serialize_parameters(const std::vector<CachedValue>& params) const;
    std::string serialize_value(const CachedValue& value) const;
};

/**
 * @brief Per-database result cache
 */
class DatabaseResultCache {
public:
    explicit DatabaseResultCache(const std::string& database_name);
    DatabaseResultCache(const std::string& database_name, const ResultCacheConfig& config);
    ~DatabaseResultCache();

    // Non-copyable
    DatabaseResultCache(const DatabaseResultCache&) = delete;
    DatabaseResultCache& operator=(const DatabaseResultCache&) = delete;

    // Cache operations
    /**
     * @brief Get cached result by SQL and parameters
     * @param sql The SQL statement
     * @param parameters Bound parameters
     * @return Cached result if found
     */
    std::shared_ptr<CachedResult> get(
        std::string_view sql,
        const std::vector<CachedValue>& parameters = {},
        uint64_t schema_version_id = 0,
        const std::string& privilege_signature = {});

    /**
     * @brief Get cached result by key hash
     * @param key_hash The cache key hash
     * @return Cached result if found
     */
    std::shared_ptr<CachedResult> get_by_hash(uint64_t key_hash);

    /**
     * @brief Put a result into the cache
     * @param result The result to cache
     * @return true if successfully cached
     */
    bool put(std::shared_ptr<CachedResult> result);

    /**
     * @brief Remove result from cache
     * @param sql The SQL statement
     * @param parameters Bound parameters
     * @return true if removed
     */
    bool remove(
        std::string_view sql,
        const std::vector<CachedValue>& parameters = {},
        uint64_t schema_version_id = 0,
        const std::string& privilege_signature = {});

    /**
     * @brief Check if result is cached
     * @param sql The SQL statement
     * @param parameters Bound parameters
     * @return true if cached
     */
    bool contains(
        std::string_view sql,
        const std::vector<CachedValue>& parameters = {},
        uint64_t schema_version_id = 0,
        const std::string& privilege_signature = {}) const;

    /**
     * @brief Clear all cached results
     */
    void clear();

    // Invalidation
    /**
     * @brief Invalidate results referencing a table
     * @param table_name The table name
     * @return Number of results invalidated
     */
    uint64_t invalidate_by_table(const std::string& table_name);

    /**
     * @brief Invalidate results referencing multiple tables
     * @param table_names The table names
     * @return Number of results invalidated
     */
    uint64_t invalidate_by_tables(const std::vector<std::string>& table_names);

    /**
     * @brief Invalidate all results
     * @return Number of results invalidated
     */
    uint64_t invalidate_all();

    /**
     * @brief Mark table as modified (batch invalidation)
     *
     * Used to batch invalidations during a transaction.
     * Call commit_invalidations() to actually invalidate.
     *
     * @param table_name The modified table
     */
    void mark_table_modified(const std::string& table_name);

    /**
     * @brief Commit pending invalidations
     * @return Number of results invalidated
     */
    uint64_t commit_invalidations();

    /**
     * @brief Rollback pending invalidations (discard markers)
     */
    void rollback_invalidations();

    // Eviction
    /**
     * @brief Manually trigger eviction to reach target memory
     * @param target_bytes Target memory after eviction
     * @return Bytes freed
     */
    uint64_t evict_to_memory(uint64_t target_bytes);

    /**
     * @brief Evict expired results
     * @return Number of results evicted
     */
    uint64_t evict_expired();

    // Statistics
    const ResultCacheStatistics& statistics() const { return stats_; }
    void reset_statistics();

    // Configuration
    const ResultCacheConfig& config() const { return config_; }
    void update_config(const ResultCacheConfig& config);

    // Info
    const std::string& database_name() const { return database_name_; }
    uint64_t size() const;
    uint64_t memory_usage() const;

private:
    // Eviction helpers
    std::string evict_one_lru();
    std::string evict_one_lfu();
    std::string evict_one_by_size();
    std::string evict_one_by_ttl();

    // Cache management
    bool should_cache(const CachedResult& result) const;
    void ensure_capacity(uint64_t needed_bytes);
    void update_statistics_on_get(bool hit);
    void update_statistics_on_put(const CachedResult& result);
    void update_statistics_on_evict();
    void update_statistics_on_invalidate(const std::string& table_name);

    std::string database_name_;
    ResultCacheConfig config_;
    ResultCacheKeyGenerator key_generator_;
    ResultCacheStatistics stats_;

    // Main cache: key -> result
    std::unordered_map<std::string, std::shared_ptr<CachedResult>> cache_;

    // Hash index
    std::unordered_map<uint64_t, std::string> hash_to_key_;

    // Table reference index for invalidation
    std::unordered_map<std::string, std::unordered_set<std::string>> table_to_keys_;

    // LRU tracking
    std::list<std::string> lru_list_;
    std::unordered_map<std::string, std::list<std::string>::iterator> lru_map_;

    // LFU tracking
    std::unordered_map<std::string, uint64_t> frequency_map_;

    // Pending invalidations (transaction boundary)
    std::unordered_set<std::string> pending_invalidation_tables_;

    mutable std::shared_mutex mutex_;
};

/**
 * @brief Global result cache manager
 */
class ResultCacheManager {
public:
    /**
     * @brief Get the singleton instance
     */
    static ResultCacheManager& instance();

    // Singleton
    ResultCacheManager(const ResultCacheManager&) = delete;
    ResultCacheManager& operator=(const ResultCacheManager&) = delete;

    /**
     * @brief Initialize the cache manager
     * @param default_config Default configuration
     */
    void initialize(const ResultCacheConfig& default_config = {});

    /**
     * @brief Shutdown the cache manager
     */
    void shutdown();

    /**
     * @brief Get or create a database cache
     * @param database_name The database name
     * @return The database's result cache
     */
    std::shared_ptr<DatabaseResultCache> get_cache(const std::string& database_name);

    /**
     * @brief Get or create a database cache with specific config
     * @param database_name The database name
     * @param config The cache configuration
     * @return The database's result cache
     */
    std::shared_ptr<DatabaseResultCache> get_cache(
        const std::string& database_name,
        const ResultCacheConfig& config);

    /**
     * @brief Remove a database cache
     * @param database_name The database name
     * @return true if removed
     */
    bool remove_cache(const std::string& database_name);

    /**
     * @brief Check if database has a cache
     * @param database_name The database name
     * @return true if cache exists
     */
    bool has_cache(const std::string& database_name) const;

    /**
     * @brief Get all database names with caches
     * @return List of database names
     */
    std::vector<std::string> get_database_names() const;

    /**
     * @brief Clear all caches
     */
    void clear_all();

    /**
     * @brief Global table invalidation
     * @param database_name The database (empty for all)
     * @param table_name The table name
     * @return Total results invalidated
     */
    uint64_t invalidate_table(const std::string& database_name, const std::string& table_name);

    /**
     * @brief Get aggregate statistics
     * @return Combined statistics for all caches
     */
    ResultCacheStatistics get_aggregate_statistics() const;

    // Configuration
    const ResultCacheConfig& default_config() const { return default_config_; }
    void set_default_config(const ResultCacheConfig& config) { default_config_ = config; }

    // Global limits
    uint64_t total_memory_usage() const;
    uint64_t total_entry_count() const;

    /**
     * @brief Apply global memory limit
     *
     * Evicts entries across all databases to stay under global limit.
     *
     * @param max_bytes Maximum total bytes
     * @return Bytes freed
     */
    uint64_t apply_global_memory_limit(uint64_t max_bytes);

private:
    ResultCacheManager() = default;
    ~ResultCacheManager() = default;

    ResultCacheConfig default_config_;
    std::unordered_map<std::string, std::shared_ptr<DatabaseResultCache>> caches_;
    mutable std::shared_mutex mutex_;
    bool initialized_ = false;
};

/**
 * @brief RAII helper for result cache access
 */
class CachedResultGuard {
public:
    CachedResultGuard() = default;
    explicit CachedResultGuard(std::shared_ptr<CachedResult> result);
    ~CachedResultGuard();

    CachedResultGuard(CachedResultGuard&&) noexcept = default;
    CachedResultGuard& operator=(CachedResultGuard&&) noexcept = default;

    // Non-copyable
    CachedResultGuard(const CachedResultGuard&) = delete;
    CachedResultGuard& operator=(const CachedResultGuard&) = delete;

    CachedResult* operator->() const { return result_.get(); }
    CachedResult& operator*() const { return *result_; }
    explicit operator bool() const { return result_ != nullptr; }

    CachedResult* get() const { return result_.get(); }
    void release() { result_.reset(); }

private:
    std::shared_ptr<CachedResult> result_;
};

/**
 * @brief Result set iterator for cached results
 *
 * Provides standard iterator interface for iterating over cached rows.
 */
class CachedResultIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = CachedRow;
    using difference_type = std::ptrdiff_t;
    using pointer = const CachedRow*;
    using reference = const CachedRow&;

    CachedResultIterator() = default;
    CachedResultIterator(const CachedResult* result, uint64_t index);

    reference operator*() const;
    pointer operator->() const;

    CachedResultIterator& operator++();
    CachedResultIterator operator++(int);

    bool operator==(const CachedResultIterator& other) const;
    bool operator!=(const CachedResultIterator& other) const;

private:
    const CachedResult* result_ = nullptr;
    uint64_t index_ = 0;
};

// Utility functions
namespace result_cache_utils {

/**
 * @brief Format cache statistics as string
 */
std::string format_statistics(const ResultCacheStatistics& stats);

/**
 * @brief Calculate memory estimate for result
 */
uint64_t estimate_result_memory(const std::vector<CachedRow>& rows);

/**
 * @brief Calculate memory for a single value
 */
uint64_t value_memory(const CachedValue& value);

/**
 * @brief Determine if result should be cached
 */
bool should_cache_result(const ResultCacheConfig& config, uint64_t row_count, uint64_t memory_bytes);

/**
 * @brief Format CachedValue for display
 */
std::string format_value(const CachedValue& value);

}  // namespace result_cache_utils

}  // namespace pool
}  // namespace scratchbird

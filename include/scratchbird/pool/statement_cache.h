#pragma once

/**
 * @file statement_cache.h
 * @brief Statement Cache for Connection Pooling
 *
 * This file defines the statement caching system that stores prepared
 * statements for reuse across connections within a database pool.
 *
 * Features:
 * - LRU/LFU/ARC eviction policies
 * - Per-database and per-connection caches
 * - Statement fingerprinting for cache key generation
 * - Automatic invalidation on schema changes
 * - Memory-bounded caching
 *
 * Part of Phase 3.6: Connection Pooling
 */

#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace scratchbird {
namespace pool {

// Forward declarations
class DatabaseStatementCache;
class CachedStatement;

/**
 * @brief Eviction policy for statement cache
 */
enum class StatementEvictionPolicy {
    LRU,  // Least Recently Used (default)
    LFU,  // Least Frequently Used
    ARC,  // Adaptive Replacement Cache
    FIFO  // First In First Out
};

/**
 * @brief Statement type classification
 */
enum class StatementType {
    SELECT,
    INSERT,
    UPDATE,
    DELETE,
    DDL,
    DML_OTHER,
    DCL,
    TCL,
    UTILITY,
    UNKNOWN
};

/**
 * @brief Statement cache entry state
 */
enum class CacheEntryState {
    VALID,
    PREPARING,
    INVALID,
    EVICTING
};

/**
 * @brief Configuration for statement cache
 */
struct StatementCacheConfig {
    // Size limits
    uint32_t max_statements = 1000;      // Maximum cached statements per database
    uint64_t max_memory_bytes = 64 * 1024 * 1024;  // 64MB default
    uint32_t max_statements_per_connection = 100;  // Per-connection limit

    // Eviction policy
    StatementEvictionPolicy eviction_policy = StatementEvictionPolicy::LRU;

    // TTL settings
    std::chrono::seconds default_ttl{3600};  // 1 hour default TTL
    std::chrono::seconds min_ttl{60};        // Minimum TTL
    std::chrono::seconds max_ttl{86400};     // Maximum TTL (24 hours)

    // Statement filtering
    bool cache_select = true;
    bool cache_insert = true;
    bool cache_update = true;
    bool cache_delete = true;
    bool cache_ddl = false;  // DDL typically not cached
    bool cache_utility = false;

    // Advanced options
    uint32_t min_execution_count_to_cache = 1;  // Cache after N executions
    uint64_t min_statement_size = 10;           // Minimum SQL length to cache
    uint64_t max_statement_size = 1024 * 1024;  // Max 1MB SQL

    // Invalidation
    bool invalidate_on_schema_change = true;
    bool invalidate_on_statistics_change = false;

    // Performance tuning
    uint32_t hash_bucket_count = 1024;
    bool enable_fingerprinting = true;
    bool normalize_whitespace = true;
    bool normalize_literals = true;  // Replace literals with placeholders for fingerprint
};

/**
 * @brief Metadata about a cached statement
 */
struct StatementMetadata {
    // Identification
    std::string sql;                    // Original SQL
    std::string fingerprint;            // Normalized fingerprint
    uint64_t fingerprint_hash = 0;      // Hash of fingerprint
    StatementType statement_type = StatementType::UNKNOWN;

    // Referenced objects
    std::vector<std::string> referenced_tables;
    std::vector<std::string> referenced_schemas;
    std::vector<std::string> referenced_functions;

    // Parameter information
    uint32_t parameter_count = 0;
    std::vector<uint32_t> parameter_types;  // OIDs

    // Result information
    uint32_t result_column_count = 0;
    std::vector<std::string> result_column_names;
    std::vector<uint32_t> result_column_types;

    // Plan information (optional)
    bool has_cached_plan = false;
    uint64_t estimated_rows = 0;
    double estimated_cost = 0.0;
};

/**
 * @brief Statistics for a single cached statement
 */
struct StatementStats {
    // Usage counters
    uint64_t hit_count = 0;
    uint64_t miss_count = 0;
    uint64_t execution_count = 0;
    uint64_t error_count = 0;

    // Timing
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_accessed;
    std::chrono::system_clock::time_point last_executed;
    std::chrono::system_clock::time_point expires_at;

    // Performance metrics
    std::chrono::microseconds total_execution_time{0};
    std::chrono::microseconds min_execution_time{0};
    std::chrono::microseconds max_execution_time{0};
    std::chrono::microseconds avg_execution_time{0};

    // Memory
    uint64_t memory_bytes = 0;
    uint64_t plan_memory_bytes = 0;
};

/**
 * @brief A cached prepared statement
 */
class CachedStatement {
public:
    CachedStatement() = default;
    CachedStatement(const std::string& sql, const StatementMetadata& metadata);
    ~CachedStatement() = default;

    // Non-copyable, movable
    CachedStatement(const CachedStatement&) = delete;
    CachedStatement& operator=(const CachedStatement&) = delete;
    CachedStatement(CachedStatement&&) noexcept = default;
    CachedStatement& operator=(CachedStatement&&) noexcept = default;

    // Identification
    const std::string& sql() const { return metadata_.sql; }
    const std::string& fingerprint() const { return metadata_.fingerprint; }
    uint64_t fingerprint_hash() const { return metadata_.fingerprint_hash; }
    StatementType statement_type() const { return metadata_.statement_type; }

    // Metadata access
    const StatementMetadata& metadata() const { return metadata_; }
    StatementMetadata& metadata() { return metadata_; }

    // Statistics access
    const StatementStats& stats() const { return stats_; }
    StatementStats& stats() { return stats_; }

    // State management
    CacheEntryState state() const { return state_; }
    void set_state(CacheEntryState state) { state_ = state; }
    bool is_valid() const { return state_ == CacheEntryState::VALID; }
    bool is_expired() const;

    // Usage tracking
    void record_hit();
    void record_miss();
    void record_execution(std::chrono::microseconds duration, bool success);

    // Internal handle (for database-specific prepared statement)
    void* internal_handle() const { return internal_handle_; }
    void set_internal_handle(void* handle) { internal_handle_ = handle; }

    // Memory management
    uint64_t memory_usage() const;
    void update_memory_usage();

    // TTL management
    void set_ttl(std::chrono::seconds ttl);
    std::chrono::seconds remaining_ttl() const;
    void refresh_ttl();

private:
    StatementMetadata metadata_;
    StatementStats stats_;
    CacheEntryState state_ = CacheEntryState::VALID;
    void* internal_handle_ = nullptr;
    std::chrono::seconds ttl_{3600};
};

/**
 * @brief Aggregate statistics for a statement cache
 */
struct CacheStatistics {
    // Size metrics
    uint64_t statement_count = 0;
    uint64_t memory_bytes = 0;
    uint64_t plan_memory_bytes = 0;

    // Hit/miss ratios
    uint64_t total_hits = 0;
    uint64_t total_misses = 0;
    double hit_ratio = 0.0;

    // Eviction metrics
    uint64_t eviction_count = 0;
    uint64_t invalidation_count = 0;
    uint64_t expiration_count = 0;

    // By statement type
    std::unordered_map<StatementType, uint64_t> count_by_type;
    std::unordered_map<StatementType, uint64_t> hits_by_type;

    // Performance
    std::chrono::microseconds avg_lookup_time{0};
    std::chrono::microseconds avg_prepare_time{0};

    // Timestamps
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_eviction;
    std::chrono::system_clock::time_point last_invalidation;
};

/**
 * @brief Statement fingerprinter for cache key generation
 *
 * Creates normalized fingerprints from SQL statements by:
 * - Normalizing whitespace
 * - Replacing literals with placeholders
 * - Standardizing case
 */
class StatementFingerprinter {
public:
    StatementFingerprinter() = default;
    explicit StatementFingerprinter(const StatementCacheConfig& config);

    /**
     * @brief Generate fingerprint from SQL
     * @param sql The SQL statement
     * @return Normalized fingerprint string
     */
    std::string fingerprint(std::string_view sql) const;

    /**
     * @brief Generate hash from fingerprint
     * @param fingerprint The fingerprint string
     * @return 64-bit hash value
     */
    uint64_t hash(std::string_view fingerprint) const;

    /**
     * @brief Generate a parameter signature string
     * @param param_types Parameter type OIDs
     * @return Stable signature string (empty if no parameters)
     */
    std::string parameter_signature(const std::vector<uint32_t>& param_types) const;

    /**
     * @brief Generate cache key from SQL + parameter types
     * @param sql The SQL statement
     * @param param_types Parameter type OIDs
     * @return Cache key string
     */
    std::string cache_key(std::string_view sql,
                          const std::vector<uint32_t>& param_types) const;

    /**
     * @brief Generate cache key from fingerprint + parameter types
     * @param fingerprint Normalized fingerprint
     * @param param_types Parameter type OIDs
     * @return Cache key string
     */
    std::string cache_key_from_fingerprint(std::string_view fingerprint,
                                           const std::vector<uint32_t>& param_types) const;

    /**
     * @brief Detect statement type from SQL
     * @param sql The SQL statement
     * @return Statement type classification
     */
    StatementType detect_type(std::string_view sql) const;

    /**
     * @brief Extract referenced tables from SQL
     * @param sql The SQL statement
     * @return List of table names
     */
    std::vector<std::string> extract_tables(std::string_view sql) const;

    // Configuration
    void set_normalize_whitespace(bool enable) { normalize_whitespace_ = enable; }
    void set_normalize_literals(bool enable) { normalize_literals_ = enable; }

private:
    std::string normalize_whitespace(std::string_view sql) const;
    std::string replace_literals(std::string_view sql) const;
    std::string normalize_case(std::string_view sql) const;

    bool normalize_whitespace_ = true;
    bool normalize_literals_ = true;
};

/**
 * @brief Per-database statement cache
 *
 * Manages cached statements for a single database, with support for
 * multiple eviction policies and automatic invalidation.
 */
class DatabaseStatementCache {
public:
    explicit DatabaseStatementCache(const std::string& database_name);
    DatabaseStatementCache(const std::string& database_name, const StatementCacheConfig& config);
    ~DatabaseStatementCache();

    // Non-copyable
    DatabaseStatementCache(const DatabaseStatementCache&) = delete;
    DatabaseStatementCache& operator=(const DatabaseStatementCache&) = delete;

    // Cache operations
    /**
     * @brief Get a cached statement by SQL
     * @param sql The SQL statement
     * @param param_types Parameter type OIDs (optional)
     * @return Cached statement if found
     */
    std::shared_ptr<CachedStatement> get(std::string_view sql);
    std::shared_ptr<CachedStatement> get(std::string_view sql,
                                         const std::vector<uint32_t>& param_types);

    /**
     * @brief Get a cached statement by fingerprint hash
     * @param hash The fingerprint hash
     * @return Cached statement if found
     */
    std::shared_ptr<CachedStatement> get_by_hash(uint64_t hash);

    /**
     * @brief Put a statement into the cache
     * @param statement The statement to cache
     * @return true if successfully cached
     */
    bool put(std::shared_ptr<CachedStatement> statement);

    /**
     * @brief Remove a statement from the cache
     * @param sql The SQL statement
     * @param param_types Parameter type OIDs (optional)
     * @return true if removed
     */
    bool remove(std::string_view sql);
    bool remove(std::string_view sql, const std::vector<uint32_t>& param_types);

    /**
     * @brief Check if statement is cached
     * @param sql The SQL statement
     * @param param_types Parameter type OIDs (optional)
     * @return true if cached
     */
    bool contains(std::string_view sql) const;
    bool contains(std::string_view sql, const std::vector<uint32_t>& param_types) const;

    /**
     * @brief Clear all cached statements
     */
    void clear();

    // Invalidation
    /**
     * @brief Invalidate statements referencing a table
     * @param table_name The table name
     * @return Number of statements invalidated
     */
    uint64_t invalidate_by_table(const std::string& table_name);

    /**
     * @brief Invalidate statements referencing a schema
     * @param schema_name The schema name
     * @return Number of statements invalidated
     */
    uint64_t invalidate_by_schema(const std::string& schema_name);

    /**
     * @brief Invalidate all statements (e.g., on major schema change)
     * @return Number of statements invalidated
     */
    uint64_t invalidate_all();

    // Eviction
    /**
     * @brief Manually trigger eviction
     * @param target_count Target statement count after eviction
     * @return Number of statements evicted
     */
    uint64_t evict(uint64_t target_count);

    /**
     * @brief Evict expired statements
     * @return Number of statements evicted
     */
    uint64_t evict_expired();

    // Statistics
    const CacheStatistics& statistics() const { return stats_; }
    void reset_statistics();

    // Configuration
    const StatementCacheConfig& config() const { return config_; }
    void update_config(const StatementCacheConfig& config);

    // Info
    const std::string& database_name() const { return database_name_; }
    uint64_t size() const;
    uint64_t memory_usage() const;

private:
    // LRU implementation details
    void promote_lru(const std::string& fingerprint);
    std::string evict_one_lru();

    // LFU implementation details
    void update_lfu_frequency(const std::string& fingerprint);
    std::string evict_one_lfu();

    // ARC implementation details
    void update_arc(const std::string& fingerprint, bool hit);
    std::string evict_one_arc();

    // Internal helpers
    bool should_cache(const CachedStatement& stmt) const;
    void update_statistics_on_get(bool hit, StatementType type);
    void update_statistics_on_put(const CachedStatement& stmt);
    void update_statistics_on_evict();

    std::string database_name_;
    StatementCacheConfig config_;
    StatementFingerprinter fingerprinter_;
    CacheStatistics stats_;

    // Main cache storage: fingerprint -> statement
    std::unordered_map<std::string, std::shared_ptr<CachedStatement>> cache_;

    // Hash index for fast lookup
    std::unordered_map<uint64_t, std::string> hash_to_fingerprint_;

    // Table reference index for invalidation
    std::unordered_map<std::string, std::vector<std::string>> table_to_fingerprints_;

    // LRU tracking
    std::list<std::string> lru_list_;
    std::unordered_map<std::string, std::list<std::string>::iterator> lru_map_;

    // LFU tracking
    std::unordered_map<std::string, uint64_t> frequency_map_;
    std::map<uint64_t, std::list<std::string>> frequency_lists_;

    // ARC tracking (simplified)
    std::list<std::string> arc_t1_;  // Recent cache
    std::list<std::string> arc_t2_;  // Frequent cache
    std::list<std::string> arc_b1_;  // Ghost for T1
    std::list<std::string> arc_b2_;  // Ghost for T2
    double arc_p_ = 0.0;             // Adaptation parameter

    mutable std::shared_mutex mutex_;
};

/**
 * @brief Global statement cache manager
 *
 * Singleton that manages statement caches for all databases.
 */
class StatementCacheManager {
public:
    /**
     * @brief Get the singleton instance
     */
    static StatementCacheManager& instance();

    // Singleton - non-copyable
    StatementCacheManager(const StatementCacheManager&) = delete;
    StatementCacheManager& operator=(const StatementCacheManager&) = delete;

    /**
     * @brief Initialize the cache manager
     * @param default_config Default configuration for new caches
     */
    void initialize(const StatementCacheConfig& default_config = {});

    /**
     * @brief Shutdown the cache manager
     */
    void shutdown();

    /**
     * @brief Get or create a database cache
     * @param database_name The database name
     * @return The database's statement cache
     */
    std::shared_ptr<DatabaseStatementCache> get_cache(const std::string& database_name);

    /**
     * @brief Get or create a database cache with specific config
     * @param database_name The database name
     * @param config The cache configuration
     * @return The database's statement cache
     */
    std::shared_ptr<DatabaseStatementCache> get_cache(
        const std::string& database_name,
        const StatementCacheConfig& config);

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
     * @brief Global invalidation by table
     * @param database_name The database (or empty for all)
     * @param table_name The table name
     * @return Total statements invalidated
     */
    uint64_t invalidate_table(const std::string& database_name, const std::string& table_name);

    /**
     * @brief Get aggregate statistics
     * @return Combined statistics for all caches
     */
    CacheStatistics get_aggregate_statistics() const;

    // Configuration
    const StatementCacheConfig& default_config() const { return default_config_; }
    void set_default_config(const StatementCacheConfig& config) { default_config_ = config; }

    // Global limits
    uint64_t total_memory_usage() const;
    uint64_t total_statement_count() const;

private:
    StatementCacheManager() = default;
    ~StatementCacheManager() = default;

    StatementCacheConfig default_config_;
    std::unordered_map<std::string, std::shared_ptr<DatabaseStatementCache>> caches_;
    mutable std::shared_mutex mutex_;
    bool initialized_ = false;
};

/**
 * @brief RAII helper for statement cache access
 */
class CachedStatementGuard {
public:
    CachedStatementGuard() = default;
    explicit CachedStatementGuard(std::shared_ptr<CachedStatement> stmt);
    ~CachedStatementGuard();

    CachedStatementGuard(CachedStatementGuard&&) noexcept = default;
    CachedStatementGuard& operator=(CachedStatementGuard&&) noexcept = default;

    // Non-copyable
    CachedStatementGuard(const CachedStatementGuard&) = delete;
    CachedStatementGuard& operator=(const CachedStatementGuard&) = delete;

    CachedStatement* operator->() const { return stmt_.get(); }
    CachedStatement& operator*() const { return *stmt_; }
    explicit operator bool() const { return stmt_ != nullptr; }

    CachedStatement* get() const { return stmt_.get(); }
    void release() { stmt_.reset(); }

private:
    std::shared_ptr<CachedStatement> stmt_;
};

// Utility functions
namespace statement_cache_utils {

/**
 * @brief Format cache statistics as string
 */
std::string format_statistics(const CacheStatistics& stats);

/**
 * @brief Calculate memory estimate for SQL statement
 */
uint64_t estimate_statement_memory(std::string_view sql);

/**
 * @brief Determine if statement should be cached based on config
 */
bool should_cache_statement(const StatementCacheConfig& config, StatementType type);

}  // namespace statement_cache_utils

}  // namespace pool
}  // namespace scratchbird

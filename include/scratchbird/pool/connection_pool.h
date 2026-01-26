/**
 * ScratchBird Connection Pool
 *
 * Alpha 3 Phase 3.6: Connection Pooling
 *
 * Built-in connection pooling with:
 * - Session, Transaction, and Statement pool modes
 * - Per-database and per-user pools
 * - Statement and result caching
 * - Health checking and validation
 * - Comprehensive statistics
 */

#ifndef SCRATCHBIRD_POOL_CONNECTION_POOL_H
#define SCRATCHBIRD_POOL_CONNECTION_POOL_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"

namespace scratchbird {
namespace pool {

// Forward declarations
class DatabasePool;
class PooledConnection;
class DatabaseStatementCache;
class DatabaseResultCache;
class ConnectionStatementCache;
class HealthChecker;
class PoolManager;

// Type aliases for backward compatibility
using StatementCache = DatabaseStatementCache;
using ResultCache = DatabaseResultCache;

// ============================================================================
// Enums and Constants
// ============================================================================

/**
 * Pool mode determines when connections are returned to the pool
 */
enum class PoolMode {
    SESSION,      // Connection bound to client for entire session
    TRANSACTION,  // Connection returned after each COMMIT/ROLLBACK
    STATEMENT     // Connection returned after each statement (highest reuse)
};

/**
 * Connection state within the pool
 */
enum class ConnectionState {
    CREATED,        // Newly created, not yet authenticated
    IDLE,           // In pool, available for use
    ACQUIRED,       // Checked out to client
    IN_TRANSACTION, // In an active transaction
    CLOSING,        // Being closed
    CLOSED          // Fully closed and removed
};

/**
 * Cache eviction policy
 */
enum class EvictionPolicy {
    LRU,   // Least Recently Used
    LFU,   // Least Frequently Used
    FIFO,  // First In First Out
    TTL    // Time To Live based
};

// ============================================================================
// Configuration Structures
// ============================================================================

/**
 * Global pool configuration
 */
struct PoolConfig {
    // Pool mode
    PoolMode mode = PoolMode::TRANSACTION;
    bool enabled = true;

    // Connection limits
    uint32_t min_idle = 5;
    uint32_t max_idle = 20;
    uint32_t max_connections = 100;
    uint32_t max_connections_per_user = 0;  // 0 = unlimited
    uint32_t max_total_connections = 500;

    // Timeouts (milliseconds unless noted)
    uint32_t acquire_timeout_ms = 30000;
    uint32_t idle_timeout_sec = 300;         // seconds
    uint32_t max_lifetime_sec = 3600;        // seconds
    uint32_t validation_timeout_ms = 5000;
    uint32_t connect_timeout_ms = 10000;

    // Validation
    bool validate_on_acquire = true;
    bool validate_on_release = false;
    uint32_t validation_interval_sec = 60;   // seconds
    std::string validation_query = "SELECT 1";
    uint32_t max_validation_failures = 3;

    // Statement cache
    bool statement_cache_enabled = true;
    uint32_t statement_cache_size = 256;     // per connection
    uint32_t statement_cache_pool_size = 1000; // shared across connections
    EvictionPolicy statement_cache_policy = EvictionPolicy::LRU;
    uint32_t statement_cache_max_params = 100;

    // Result cache
    bool result_cache_enabled = true;
    size_t result_cache_size = 64 * 1024 * 1024;  // 64MB
    size_t result_cache_max_entry = 1024 * 1024;  // 1MB
    uint32_t result_cache_ttl_sec = 300;          // seconds
    EvictionPolicy result_cache_policy = EvictionPolicy::LRU;
    std::string result_cache_exclude;             // comma-separated patterns
    bool result_cache_invalidate_on_dml = true;

    // Pre-warming
    bool prewarm = true;
    uint32_t prewarm_count = 5;
    std::string prewarm_queries = "SELECT 1";

    // Monitoring
    bool stats_enabled = true;
    uint32_t stats_interval_sec = 10;
    bool log_pool_stats = true;
    uint32_t log_slow_acquire_ms = 1000;  // 0 = disabled

    // Debug
    bool debug = false;
    bool log_acquire_release = false;
    bool log_cache_operations = false;
    bool log_validations = false;
};

/**
 * Per-database pool configuration override
 */
struct DatabasePoolConfig {
    std::string database_name;

    // Overrides (0 = use global)
    uint32_t max_connections = 0;
    uint32_t min_idle = 0;
    uint32_t max_idle = 0;
    uint32_t acquire_timeout_ms = 0;
    uint32_t idle_timeout_sec = 0;
    uint32_t max_lifetime_sec = 0;
    uint32_t statement_cache_size = 0;
    size_t result_cache_size = 0;
    bool result_cache_enabled = true;
};

/**
 * Connection configuration for creating new connections
 */
struct ConnectionConfig {
    std::string host;
    uint16_t port = 3092;
    std::string database;
    std::string user;
    std::string password;
    std::string application_name = "scratchbird_pool";

    // SSL configuration
    bool use_ssl = false;
    std::string ssl_cert;
    std::string ssl_key;
    std::string ssl_ca;

    // Timeouts
    uint32_t connect_timeout_ms = 10000;
    uint32_t socket_timeout_ms = 0;  // 0 = no timeout
};

// ============================================================================
// Statistics Structures
// ============================================================================

/**
 * Statement cache statistics
 */
struct StatementCacheStats {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> inserts{0};
    std::atomic<uint64_t> evictions{0};
    std::atomic<uint64_t> invalidations{0};
    std::atomic<uint64_t> current_size{0};
    std::atomic<uint64_t> current_memory{0};

    double hitRatio() const {
        uint64_t total = hits.load() + misses.load();
        return total > 0 ? static_cast<double>(hits.load()) / total : 0.0;
    }
};

/**
 * Result cache statistics
 */
struct ResultCacheStats {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> stale_hits{0};
    std::atomic<uint64_t> inserts{0};
    std::atomic<uint64_t> evictions{0};
    std::atomic<uint64_t> invalidations{0};
    std::atomic<uint64_t> too_large{0};
    std::atomic<uint64_t> clears{0};
    std::atomic<uint64_t> current_size{0};
    std::atomic<uint64_t> current_memory{0};

    double hitRatio() const {
        uint64_t total = hits.load() + misses.load();
        return total > 0 ? static_cast<double>(hits.load()) / total : 0.0;
    }
};

/**
 * Health check statistics
 */
struct HealthCheckStats {
    std::atomic<uint64_t> validations{0};
    std::atomic<uint64_t> validation_failures{0};
    std::atomic<uint64_t> removed{0};
    std::atomic<uint64_t> recovered{0};
};

/**
 * Pool statistics
 */
struct PoolStatistics {
    // Connection counts
    std::atomic<uint64_t> total_connections{0};
    std::atomic<uint64_t> active_connections{0};
    std::atomic<uint64_t> idle_connections{0};
    std::atomic<uint64_t> pending_requests{0};

    // Acquisition metrics
    std::atomic<uint64_t> acquires{0};
    std::atomic<uint64_t> releases{0};
    std::atomic<uint64_t> creates{0};
    std::atomic<uint64_t> closes{0};
    std::atomic<uint64_t> timeouts{0};
    std::atomic<uint64_t> waits{0};

    // Timing metrics (microseconds)
    std::atomic<uint64_t> total_acquire_time_us{0};
    std::atomic<uint64_t> total_wait_time_us{0};
    std::atomic<uint64_t> max_acquire_time_us{0};
    std::atomic<uint64_t> max_wait_time_us{0};

    // Cache statistics
    StatementCacheStats stmt_cache;
    ResultCacheStats result_cache;

    // Health check statistics
    HealthCheckStats health_check;

    // Timestamps
    std::chrono::steady_clock::time_point start_time;

    // Calculated metrics
    double acquiresPerSecond() const;
    double avgAcquireTimeUs() const;
    double utilization() const;
};

// ============================================================================
// Snapshot Structures (non-atomic, copyable/movable)
// ============================================================================

/**
 * Statement cache statistics snapshot (copyable)
 */
struct StatementCacheStatsSnapshot {
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t inserts = 0;
    uint64_t evictions = 0;
    uint64_t invalidations = 0;
    uint64_t current_size = 0;
    uint64_t current_memory = 0;

    double hitRatio() const {
        uint64_t total = hits + misses;
        return total > 0 ? static_cast<double>(hits) / total : 0.0;
    }

    static StatementCacheStatsSnapshot from(const StatementCacheStats& s) {
        StatementCacheStatsSnapshot snap;
        snap.hits = s.hits.load();
        snap.misses = s.misses.load();
        snap.inserts = s.inserts.load();
        snap.evictions = s.evictions.load();
        snap.invalidations = s.invalidations.load();
        snap.current_size = s.current_size.load();
        snap.current_memory = s.current_memory.load();
        return snap;
    }
};

/**
 * Result cache statistics snapshot (copyable)
 */
struct ResultCacheStatsSnapshot {
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t stale_hits = 0;
    uint64_t inserts = 0;
    uint64_t evictions = 0;
    uint64_t invalidations = 0;
    uint64_t too_large = 0;
    uint64_t clears = 0;
    uint64_t current_size = 0;
    uint64_t current_memory = 0;

    double hitRatio() const {
        uint64_t total = hits + misses;
        return total > 0 ? static_cast<double>(hits) / total : 0.0;
    }

    static ResultCacheStatsSnapshot from(const ResultCacheStats& s) {
        ResultCacheStatsSnapshot snap;
        snap.hits = s.hits.load();
        snap.misses = s.misses.load();
        snap.stale_hits = s.stale_hits.load();
        snap.inserts = s.inserts.load();
        snap.evictions = s.evictions.load();
        snap.invalidations = s.invalidations.load();
        snap.too_large = s.too_large.load();
        snap.clears = s.clears.load();
        snap.current_size = s.current_size.load();
        snap.current_memory = s.current_memory.load();
        return snap;
    }
};

/**
 * Health check statistics snapshot (copyable)
 */
struct HealthCheckStatsSnapshot {
    uint64_t validations = 0;
    uint64_t validation_failures = 0;
    uint64_t removed = 0;
    uint64_t recovered = 0;

    static HealthCheckStatsSnapshot from(const HealthCheckStats& s) {
        HealthCheckStatsSnapshot snap;
        snap.validations = s.validations.load();
        snap.validation_failures = s.validation_failures.load();
        snap.removed = s.removed.load();
        snap.recovered = s.recovered.load();
        return snap;
    }
};

/**
 * Pool statistics snapshot (copyable/movable)
 * Used for returning statistics from functions
 */
struct PoolStatisticsSnapshot {
    // Connection counts
    uint64_t total_connections = 0;
    uint64_t active_connections = 0;
    uint64_t idle_connections = 0;
    uint64_t pending_requests = 0;

    // Acquisition metrics
    uint64_t acquires = 0;
    uint64_t releases = 0;
    uint64_t creates = 0;
    uint64_t closes = 0;
    uint64_t timeouts = 0;
    uint64_t waits = 0;

    // Timing metrics (microseconds)
    uint64_t total_acquire_time_us = 0;
    uint64_t total_wait_time_us = 0;
    uint64_t max_acquire_time_us = 0;
    uint64_t max_wait_time_us = 0;

    // Cache statistics
    StatementCacheStatsSnapshot stmt_cache;
    ResultCacheStatsSnapshot result_cache;

    // Health check statistics
    HealthCheckStatsSnapshot health_check;

    // Timestamps
    std::chrono::steady_clock::time_point start_time;

    // Calculated metrics
    double acquiresPerSecond() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        return elapsed > 0 ? static_cast<double>(acquires) / elapsed : 0.0;
    }

    double avgAcquireTimeUs() const {
        return acquires > 0 ? static_cast<double>(total_acquire_time_us) / acquires : 0.0;
    }

    double utilization() const {
        return total_connections > 0 ?
            static_cast<double>(active_connections) / total_connections : 0.0;
    }

    static PoolStatisticsSnapshot from(const PoolStatistics& s) {
        PoolStatisticsSnapshot snap;
        snap.total_connections = s.total_connections.load();
        snap.active_connections = s.active_connections.load();
        snap.idle_connections = s.idle_connections.load();
        snap.pending_requests = s.pending_requests.load();
        snap.acquires = s.acquires.load();
        snap.releases = s.releases.load();
        snap.creates = s.creates.load();
        snap.closes = s.closes.load();
        snap.timeouts = s.timeouts.load();
        snap.waits = s.waits.load();
        snap.total_acquire_time_us = s.total_acquire_time_us.load();
        snap.total_wait_time_us = s.total_wait_time_us.load();
        snap.max_acquire_time_us = s.max_acquire_time_us.load();
        snap.max_wait_time_us = s.max_wait_time_us.load();
        snap.stmt_cache = StatementCacheStatsSnapshot::from(s.stmt_cache);
        snap.result_cache = ResultCacheStatsSnapshot::from(s.result_cache);
        snap.health_check = HealthCheckStatsSnapshot::from(s.health_check);
        snap.start_time = s.start_time;
        return snap;
    }
};

// ============================================================================
// PooledConnection
// ============================================================================

/**
 * Wrapper around a database connection with pool metadata
 */
class PooledConnection {
public:
    PooledConnection();
    ~PooledConnection();

    // Non-copyable, movable
    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;
    PooledConnection(PooledConnection&&) noexcept;
    PooledConnection& operator=(PooledConnection&&) noexcept;

    // Connection operations
    core::Status connect(const ConnectionConfig& config, core::ErrorContext* ctx = nullptr);
    core::Status execute(const std::string& sql, core::ErrorContext* ctx = nullptr);
    core::Status executeWithParams(const std::string& sql,
                                   const std::vector<std::string>& params,
                                   core::ErrorContext* ctx = nullptr);
    void close();

    // State management
    void markBroken();
    void markNeedsReset();
    bool isValid() const;
    bool validate(const std::string& query, uint32_t timeout_ms);

    // Reset connection state
    void fastReset();
    void fullReset();

    // Tagging for connection routing
    void setTag(const std::string& key, const std::string& value);
    std::string getTag(const std::string& key) const;
    void clearTags();
    const std::map<std::string, std::string>& tags() const { return tags_; }

    // Affinity for session binding
    void setAffinity(const std::string& client_id);
    const std::string& affinity() const { return affinity_; }
    void clearAffinity();
    bool hasAffinity() const { return !affinity_.empty(); }

    // Accessors
    uint64_t id() const { return id_; }
    ConnectionState state() const { return state_; }
    void setState(ConnectionState state) { state_ = state; }
    const std::string& user() const { return user_; }
    const std::string& database() const { return database_; }
    bool inTransaction() const { return in_transaction_; }
    void setInTransaction(bool v) { in_transaction_ = v; }
    bool needsReset() const { return needs_reset_; }
    bool isBroken() const { return is_broken_; }

    // Timestamps
    std::chrono::steady_clock::time_point createdAt() const { return created_at_; }
    std::chrono::steady_clock::time_point lastUsed() const { return last_used_; }
    std::chrono::steady_clock::time_point idleSince() const { return idle_since_; }
    std::chrono::steady_clock::time_point lastValidated() const { return last_validated_; }
    void updateLastUsed() { last_used_ = std::chrono::steady_clock::now(); }
    void updateIdleSince() { idle_since_ = std::chrono::steady_clock::now(); }
    void updateLastValidated() { last_validated_ = std::chrono::steady_clock::now(); }

    // Statistics
    uint64_t queriesExecuted() const { return queries_executed_; }
    uint64_t transactionsCompleted() const { return transactions_completed_; }
    uint64_t bytesSent() const { return bytes_sent_; }
    uint64_t bytesReceived() const { return bytes_received_; }
    uint32_t validationFailures() const { return validation_failures_; }
    void incrementQueries() { ++queries_executed_; }
    void incrementTransactions() { ++transactions_completed_; }
    void addBytesSent(uint64_t bytes) { bytes_sent_ += bytes; }
    void addBytesReceived(uint64_t bytes) { bytes_received_ += bytes; }
    void incrementValidationFailures() { ++validation_failures_; }
    void resetValidationFailures() { validation_failures_ = 0; }

    // Pool association
    DatabasePool* pool() const { return pool_; }
    void setPool(DatabasePool* pool) { pool_ = pool; }

    // Statement cache reference
    StatementCache* statementCache() const { return stmt_cache_; }
    void setStatementCache(StatementCache* cache) { stmt_cache_ = cache; }
    ConnectionStatementCache* sessionStatementCache() const { return session_stmt_cache_.get(); }
    void setSessionStatementCache(std::unique_ptr<ConnectionStatementCache> cache) {
        session_stmt_cache_ = std::move(cache);
    }
    void clearSessionStatementCache();

private:
    static std::atomic<uint64_t> next_id_;

    uint64_t id_;
    ConnectionState state_ = ConnectionState::CREATED;

    // Pool membership
    DatabasePool* pool_ = nullptr;
    std::string user_;
    std::string database_;

    // State tracking
    bool in_transaction_ = false;
    bool needs_reset_ = false;
    bool is_broken_ = false;
    bool security_context_changed_ = false;

    // Timestamps
    std::chrono::steady_clock::time_point created_at_;
    std::chrono::steady_clock::time_point last_used_;
    std::chrono::steady_clock::time_point idle_since_;
    std::chrono::steady_clock::time_point last_validated_;

    // Statistics
    uint64_t queries_executed_ = 0;
    uint64_t transactions_completed_ = 0;
    uint64_t bytes_sent_ = 0;
    uint64_t bytes_received_ = 0;
    uint32_t validation_failures_ = 0;

    // Statement cache
    StatementCache* stmt_cache_ = nullptr;
    std::unique_ptr<ConnectionStatementCache> session_stmt_cache_;

    // Tagging and affinity
    std::map<std::string, std::string> tags_;
    std::string affinity_;

    // Underlying connection handle (opaque)
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// DatabasePool
// ============================================================================

/**
 * Connection pool for a specific database
 */
class DatabasePool {
public:
    DatabasePool(const std::string& database_name,
                 const PoolConfig& global_config,
                 const DatabasePoolConfig& db_config = {});
    ~DatabasePool();

    // Non-copyable
    DatabasePool(const DatabasePool&) = delete;
    DatabasePool& operator=(const DatabasePool&) = delete;

    // Connection acquisition
    PooledConnection* acquire(const std::string& user,
                              std::chrono::milliseconds timeout,
                              core::ErrorContext* ctx = nullptr);

    PooledConnection* acquireWithTags(const std::string& user,
                                      const std::map<std::string, std::string>& required_tags,
                                      std::chrono::milliseconds timeout,
                                      core::ErrorContext* ctx = nullptr);

    PooledConnection* acquireWithAffinity(const std::string& client_id,
                                          const std::string& user,
                                          std::chrono::milliseconds timeout,
                                          core::ErrorContext* ctx = nullptr);

    // Connection release
    void release(PooledConnection* conn);

    // Pool management
    core::Status prewarm(uint32_t count, core::ErrorContext* ctx = nullptr);
    void drain();
    void resume();
    void forceCloseAll();
    bool waitForIdle(std::chrono::milliseconds timeout);

    // Connection management
    void removeConnection(PooledConnection* conn);
    void markConnectionBroken(PooledConnection* conn);

    // Cache management
    StatementCache* statementCache() { return stmt_cache_.get(); }
    ResultCache* resultCache() { return result_cache_.get(); }
    void clearStatementCache();
    void clearResultCache();
    void invalidateCacheForTable(const std::string& table_name);

    // Accessors
    const std::string& name() const { return database_name_; }
    PoolMode mode() const { return effective_config_.mode; }
    uint32_t maxConnections() const { return effective_config_.max_connections; }
    uint32_t minIdle() const { return effective_config_.min_idle; }
    uint32_t maxIdle() const { return effective_config_.max_idle; }
    bool isDraining() const { return draining_; }

    // Statistics
    const PoolStatistics& statistics() const { return stats_; }
    PoolStatistics& statistics() { return stats_; }
    std::vector<PooledConnection*> getIdleConnections();
    std::vector<PooledConnection*> getAllConnections();
    uint32_t activeConnectionCount() const { return stats_.active_connections.load(); }
    uint32_t idleConnectionCount() const { return stats_.idle_connections.load(); }
    uint32_t pendingRequestCount() const { return stats_.pending_requests.load(); }

    // Configuration update (hot reload)
    void updateConfig(const DatabasePoolConfig& config);

private:
    PooledConnection* tryGetIdleConnection(const std::string& user);
    PooledConnection* tryGetConnectionWithTags(const std::string& user,
                                               const std::map<std::string, std::string>& tags);
    PooledConnection* tryGetConnectionWithAffinity(const std::string& client_id);
    PooledConnection* createConnection(const std::string& user, core::ErrorContext* ctx);
    bool shouldClose(PooledConnection* conn);
    void closeConnection(PooledConnection* conn);
    void resetConnection(PooledConnection* conn);

    std::string database_name_;
    PoolConfig effective_config_;  // Merged global + db-specific

    // Connection storage
    std::vector<std::unique_ptr<PooledConnection>> all_connections_;
    std::vector<PooledConnection*> idle_connections_;

    // Synchronization
    mutable std::mutex mutex_;
    std::condition_variable available_;

    // State
    std::atomic<bool> draining_{false};
    std::atomic<bool> shutdown_{false};

    // Caches
    std::unique_ptr<StatementCache> stmt_cache_;
    std::unique_ptr<ResultCache> result_cache_;

    // Statistics
    PoolStatistics stats_;
};

// ============================================================================
// PoolManager
// ============================================================================

/**
 * Global connection pool manager (singleton)
 */
class PoolManager {
public:
    // Singleton access
    static PoolManager& instance();

    // Initialization and shutdown
    core::Status initialize(const PoolConfig& config, core::ErrorContext* ctx = nullptr);
    core::Status shutdown(std::chrono::seconds timeout = std::chrono::seconds(30),
                          core::ErrorContext* ctx = nullptr);
    bool isInitialized() const { return initialized_; }

    // Pool access
    DatabasePool* getPool(const std::string& database);
    DatabasePool* getOrCreatePool(const std::string& database,
                                  const DatabasePoolConfig& config = {});
    std::vector<std::string> poolNames() const;

    // Connection acquisition (convenience)
    PooledConnection* acquire(const std::string& database,
                              const std::string& user,
                              std::chrono::milliseconds timeout = std::chrono::milliseconds(30000),
                              core::ErrorContext* ctx = nullptr);

    void release(PooledConnection* conn);

    // Global configuration
    const PoolConfig& config() const { return global_config_; }
    void setDatabaseConfig(const std::string& database, const DatabasePoolConfig& config);

    // Global cache operations
    void clearAllStatementCaches();
    void clearAllResultCaches();
    void invalidateTableInAllPools(const std::string& table_name);

    // Statistics (return copyable snapshots)
    PoolStatisticsSnapshot aggregateStatistics() const;
    std::map<std::string, PoolStatisticsSnapshot> allPoolStatistics() const;

    // Health checking
    void startHealthChecker();
    void stopHealthChecker();

    // Background maintenance
    void startEvictor();
    void stopEvictor();

    // Debug/diagnostics
    void logPoolStatus();
    std::string getPoolStatusJson() const;

private:
    PoolManager() = default;
    ~PoolManager();

    // Non-copyable
    PoolManager(const PoolManager&) = delete;
    PoolManager& operator=(const PoolManager&) = delete;

    // Background thread loops
    void healthCheckLoop();
    void evictionLoop();
    void statsLoop();

    bool initialized_ = false;
    std::atomic<bool> shutdown_flag_{false};
    PoolConfig global_config_;

    // Pool registry
    mutable std::shared_mutex pools_mutex_;
    std::unordered_map<std::string, std::unique_ptr<DatabasePool>> pools_;

    // Per-database configuration overrides
    std::map<std::string, DatabasePoolConfig> db_configs_;

    // Background threads
    std::unique_ptr<std::thread> health_checker_thread_;
    std::unique_ptr<std::thread> evictor_thread_;
    std::unique_ptr<std::thread> stats_thread_;

    // Thread synchronization
    std::mutex health_mutex_;
    std::condition_variable health_cv_;
    std::mutex evictor_mutex_;
    std::condition_variable evictor_cv_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Convert pool mode to string
 */
const char* poolModeToString(PoolMode mode);

/**
 * Parse pool mode from string
 */
bool parsePoolMode(const std::string& str, PoolMode& mode);

/**
 * Convert connection state to string
 */
const char* connectionStateToString(ConnectionState state);

/**
 * Convert eviction policy to string
 */
const char* evictionPolicyToString(EvictionPolicy policy);

/**
 * Parse eviction policy from string
 */
bool parseEvictionPolicy(const std::string& str, EvictionPolicy& policy);

/**
 * RAII wrapper for acquiring a pooled connection
 */
class PooledConnectionGuard {
public:
    PooledConnectionGuard(PoolManager& manager,
                          const std::string& database,
                          const std::string& user,
                          std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));

    PooledConnectionGuard(DatabasePool& pool,
                          const std::string& user,
                          std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));

    ~PooledConnectionGuard();

    // Non-copyable, movable
    PooledConnectionGuard(const PooledConnectionGuard&) = delete;
    PooledConnectionGuard& operator=(const PooledConnectionGuard&) = delete;
    PooledConnectionGuard(PooledConnectionGuard&& other) noexcept;
    PooledConnectionGuard& operator=(PooledConnectionGuard&& other) noexcept;

    // Access connection
    PooledConnection* get() const { return conn_; }
    PooledConnection* operator->() const { return conn_; }
    PooledConnection& operator*() const { return *conn_; }

    // Check validity
    bool valid() const { return conn_ != nullptr; }
    explicit operator bool() const { return valid(); }

    // Release ownership (connection will not be returned to pool)
    PooledConnection* release();

private:
    PooledConnection* conn_ = nullptr;
    DatabasePool* pool_ = nullptr;
    PoolManager* manager_ = nullptr;
};

}  // namespace pool
}  // namespace scratchbird

#endif  // SCRATCHBIRD_POOL_CONNECTION_POOL_H

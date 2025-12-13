/**
 * @file connection_pool.cpp
 * @brief Connection Pool Implementation
 *
 * Implements the connection pooling system with three modes:
 * - Session pooling: Connections bound to client sessions
 * - Transaction pooling: Connections returned after each transaction
 * - Statement pooling: Connections returned after each statement
 *
 * Part of Phase 3.6: Connection Pooling
 */

#include "scratchbird/pool/connection_pool.h"
#include "scratchbird/pool/statement_cache.h"
#include "scratchbird/pool/result_cache.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>

namespace scratchbird {
namespace pool {

// =============================================================================
// PooledConnection::Impl Definition
// =============================================================================

/**
 * Private implementation details for PooledConnection.
 * Contains the actual connection handle and internal state.
 */
struct PooledConnection::Impl {
    // Connection handle (placeholder - would be actual IPC/TCP socket in full implementation)
    int socket_fd = -1;
    bool connected = false;
    std::string connection_string;

    Impl() = default;
    ~Impl() {
        if (socket_fd >= 0) {
            // Would close socket here
            socket_fd = -1;
        }
        connected = false;
    }
};

// =============================================================================
// Static Initialization
// =============================================================================

std::atomic<uint64_t> PooledConnection::next_id_{0};

// =============================================================================
// PoolStatistics Implementation
// =============================================================================

double PoolStatistics::acquiresPerSecond() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    if (duration.count() == 0) return 0.0;
    return static_cast<double>(acquires.load()) / duration.count();
}

double PoolStatistics::avgAcquireTimeUs() const {
    uint64_t total_acquires = acquires.load();
    if (total_acquires == 0) return 0.0;
    return static_cast<double>(total_acquire_time_us.load()) / total_acquires;
}

double PoolStatistics::utilization() const {
    uint64_t total = total_connections.load();
    if (total == 0) return 0.0;
    return static_cast<double>(active_connections.load()) / total;
}

// =============================================================================
// PooledConnection Implementation
// =============================================================================

PooledConnection::PooledConnection()
    : id_(++next_id_)
    , state_(ConnectionState::CREATED)
    , created_at_(std::chrono::steady_clock::now())
    , last_used_(created_at_)
    , idle_since_(created_at_)
    , last_validated_(created_at_) {
}

PooledConnection::~PooledConnection() {
    close();
}

PooledConnection::PooledConnection(PooledConnection&& other) noexcept
    : id_(other.id_)
    , state_(other.state_)
    , pool_(other.pool_)
    , user_(std::move(other.user_))
    , database_(std::move(other.database_))
    , in_transaction_(other.in_transaction_)
    , needs_reset_(other.needs_reset_)
    , is_broken_(other.is_broken_)
    , security_context_changed_(other.security_context_changed_)
    , created_at_(other.created_at_)
    , last_used_(other.last_used_)
    , idle_since_(other.idle_since_)
    , last_validated_(other.last_validated_)
    , queries_executed_(other.queries_executed_)
    , transactions_completed_(other.transactions_completed_)
    , bytes_sent_(other.bytes_sent_)
    , bytes_received_(other.bytes_received_)
    , validation_failures_(other.validation_failures_)
    , stmt_cache_(other.stmt_cache_)
    , tags_(std::move(other.tags_))
    , affinity_(std::move(other.affinity_)) {
    other.pool_ = nullptr;
    other.stmt_cache_ = nullptr;
}

PooledConnection& PooledConnection::operator=(PooledConnection&& other) noexcept {
    if (this != &other) {
        close();

        id_ = other.id_;
        state_ = other.state_;
        pool_ = other.pool_;
        user_ = std::move(other.user_);
        database_ = std::move(other.database_);
        in_transaction_ = other.in_transaction_;
        needs_reset_ = other.needs_reset_;
        is_broken_ = other.is_broken_;
        security_context_changed_ = other.security_context_changed_;
        created_at_ = other.created_at_;
        last_used_ = other.last_used_;
        idle_since_ = other.idle_since_;
        last_validated_ = other.last_validated_;
        queries_executed_ = other.queries_executed_;
        transactions_completed_ = other.transactions_completed_;
        bytes_sent_ = other.bytes_sent_;
        bytes_received_ = other.bytes_received_;
        validation_failures_ = other.validation_failures_;
        stmt_cache_ = other.stmt_cache_;
        tags_ = std::move(other.tags_);
        affinity_ = std::move(other.affinity_);

        other.pool_ = nullptr;
        other.stmt_cache_ = nullptr;
    }
    return *this;
}

core::Status PooledConnection::connect(const ConnectionConfig& config, core::ErrorContext* ctx) {
    database_ = config.database;
    user_ = config.user;

    // TODO: Implement actual connection logic
    // For now, simulate successful connection
    state_ = ConnectionState::IDLE;
    last_validated_ = std::chrono::steady_clock::now();

    return core::Status::OK;
}

core::Status PooledConnection::execute(const std::string& sql, core::ErrorContext* ctx) {
    if (state_ == ConnectionState::CLOSED || is_broken_) {
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE, "Connection is closed or broken");
        return core::Status::CONNECTION_FAILURE;
    }

    // TODO: Implement actual SQL execution
    ++queries_executed_;
    last_used_ = std::chrono::steady_clock::now();

    return core::Status::OK;
}

core::Status PooledConnection::executeWithParams(const std::string& sql,
                                                  const std::vector<std::string>& params,
                                                  core::ErrorContext* ctx) {
    if (state_ == ConnectionState::CLOSED || is_broken_) {
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE, "Connection is closed or broken");
        return core::Status::CONNECTION_FAILURE;
    }

    // TODO: Implement actual parameterized SQL execution
    ++queries_executed_;
    last_used_ = std::chrono::steady_clock::now();

    return core::Status::OK;
}

void PooledConnection::close() {
    if (state_ == ConnectionState::CLOSED) {
        return;
    }

    state_ = ConnectionState::CLOSING;

    // TODO: Close actual connection

    state_ = ConnectionState::CLOSED;
}

void PooledConnection::markBroken() {
    is_broken_ = true;
}

void PooledConnection::markNeedsReset() {
    needs_reset_ = true;
}

bool PooledConnection::isValid() const {
    return state_ != ConnectionState::CLOSED &&
           state_ != ConnectionState::CLOSING &&
           !is_broken_;
}

bool PooledConnection::validate(const std::string& query, uint32_t timeout_ms) {
    if (!isValid()) {
        return false;
    }

    // TODO: Execute validation query with timeout

    last_validated_ = std::chrono::steady_clock::now();
    return true;
}

void PooledConnection::fastReset() {
    // Reset minimal state
    in_transaction_ = false;
    needs_reset_ = false;
    security_context_changed_ = false;
    last_used_ = std::chrono::steady_clock::now();
}

void PooledConnection::fullReset() {
    fastReset();

    // Full reset includes clearing tags and state
    tags_.clear();

    // TODO: Reset session variables, search path, etc.
}

void PooledConnection::setTag(const std::string& key, const std::string& value) {
    tags_[key] = value;
}

std::string PooledConnection::getTag(const std::string& key) const {
    auto it = tags_.find(key);
    if (it != tags_.end()) {
        return it->second;
    }
    return "";
}

void PooledConnection::clearTags() {
    tags_.clear();
}

void PooledConnection::setAffinity(const std::string& client_id) {
    affinity_ = client_id;
}

void PooledConnection::clearAffinity() {
    affinity_.clear();
}

// =============================================================================
// DatabasePool Implementation
// =============================================================================

DatabasePool::DatabasePool(const std::string& database_name,
                           const PoolConfig& global_config,
                           const DatabasePoolConfig& db_config)
    : database_name_(database_name)
    , effective_config_(global_config) {
    // Apply database-specific overrides
    if (db_config.max_connections > 0) {
        effective_config_.max_connections = db_config.max_connections;
    }
    if (db_config.min_idle > 0) {
        effective_config_.min_idle = db_config.min_idle;
    }
    if (db_config.max_idle > 0) {
        effective_config_.max_idle = db_config.max_idle;
    }
    if (db_config.acquire_timeout_ms > 0) {
        effective_config_.acquire_timeout_ms = db_config.acquire_timeout_ms;
    }
    if (db_config.idle_timeout_sec > 0) {
        effective_config_.idle_timeout_sec = db_config.idle_timeout_sec;
    }
    if (db_config.max_lifetime_sec > 0) {
        effective_config_.max_lifetime_sec = db_config.max_lifetime_sec;
    }
    if (db_config.statement_cache_size > 0) {
        effective_config_.statement_cache_size = db_config.statement_cache_size;
    }
    if (db_config.result_cache_size > 0) {
        effective_config_.result_cache_size = db_config.result_cache_size;
    }
    effective_config_.result_cache_enabled = db_config.result_cache_enabled;

    stats_.start_time = std::chrono::steady_clock::now();

    // Initialize caches
    // TODO: Create actual cache instances
}

DatabasePool::~DatabasePool() {
    forceCloseAll();
}

PooledConnection* DatabasePool::acquire(const std::string& user,
                                         std::chrono::milliseconds timeout,
                                         core::ErrorContext* ctx) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    auto start_time = std::chrono::steady_clock::now();

    std::unique_lock<std::mutex> lock(mutex_);

    stats_.pending_requests++;

    while (true) {
        // Check if draining
        if (draining_) {
            stats_.pending_requests--;
            SET_ERROR_CONTEXT(ctx, core::Status::CANCELLED, "Pool is draining");
            return nullptr;
        }

        // Try to get an idle connection
        auto* conn = tryGetIdleConnection(user);
        if (conn) {
            stats_.pending_requests--;
            stats_.acquires++;
            stats_.active_connections++;
            stats_.idle_connections--;

            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start_time);
            stats_.total_acquire_time_us += elapsed.count();
            uint64_t elapsed_count = elapsed.count();
            uint64_t current_max = stats_.max_acquire_time_us.load();
            while (elapsed_count > current_max &&
                   !stats_.max_acquire_time_us.compare_exchange_weak(current_max, elapsed_count)) {
            }

            conn->setState(ConnectionState::ACQUIRED);
            conn->updateLastUsed();
            return conn;
        }

        // Try to create a new connection if under limit
        if (all_connections_.size() < effective_config_.max_connections) {
            lock.unlock();
            conn = createConnection(user, ctx);
            lock.lock();

            if (conn) {
                stats_.pending_requests--;
                stats_.acquires++;
                stats_.active_connections++;
                stats_.creates++;
                stats_.total_connections++;

                auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - start_time);
                stats_.total_acquire_time_us += elapsed.count();
                uint64_t elapsed_count = elapsed.count();
                uint64_t current_max = stats_.max_acquire_time_us.load();
                while (elapsed_count > current_max &&
                       !stats_.max_acquire_time_us.compare_exchange_weak(current_max, elapsed_count)) {
                }

                conn->setState(ConnectionState::ACQUIRED);
                conn->updateLastUsed();
                return conn;
            }
        }

        // Wait for a connection to become available
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            stats_.pending_requests--;
            stats_.timeouts++;
            SET_ERROR_CONTEXT(ctx, core::Status::LOCK_TIMEOUT, "Timeout waiting for connection");
            return nullptr;
        }

        stats_.waits++;
        auto wait_start = std::chrono::steady_clock::now();

        available_.wait_until(lock, deadline);

        auto wait_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - wait_start);
        stats_.total_wait_time_us += wait_elapsed.count();
        uint64_t wait_count = wait_elapsed.count();
        uint64_t current_max_wait = stats_.max_wait_time_us.load();
        while (wait_count > current_max_wait &&
               !stats_.max_wait_time_us.compare_exchange_weak(current_max_wait, wait_count)) {
        }
    }
}

PooledConnection* DatabasePool::acquireWithTags(
    const std::string& user,
    const std::map<std::string, std::string>& required_tags,
    std::chrono::milliseconds timeout,
    core::ErrorContext* ctx) {

    std::unique_lock<std::mutex> lock(mutex_);

    // First, try to find a connection with matching tags
    auto* conn = tryGetConnectionWithTags(user, required_tags);
    if (conn) {
        stats_.acquires++;
        stats_.active_connections++;
        stats_.idle_connections--;

        conn->setState(ConnectionState::ACQUIRED);
        conn->updateLastUsed();
        return conn;
    }

    lock.unlock();

    // Fall back to regular acquire
    conn = acquire(user, timeout, ctx);
    if (conn) {
        // Set the required tags
        for (const auto& pair : required_tags) {
            conn->setTag(pair.first, pair.second);
        }
    }
    return conn;
}

PooledConnection* DatabasePool::acquireWithAffinity(
    const std::string& client_id,
    const std::string& user,
    std::chrono::milliseconds timeout,
    core::ErrorContext* ctx) {

    std::unique_lock<std::mutex> lock(mutex_);

    // First, try to find a connection with matching affinity
    auto* conn = tryGetConnectionWithAffinity(client_id);
    if (conn) {
        stats_.acquires++;
        stats_.active_connections++;
        stats_.idle_connections--;

        conn->setState(ConnectionState::ACQUIRED);
        conn->updateLastUsed();
        return conn;
    }

    lock.unlock();

    // Fall back to regular acquire
    conn = acquire(user, timeout, ctx);
    if (conn) {
        conn->setAffinity(client_id);
    }
    return conn;
}

void DatabasePool::release(PooledConnection* conn) {
    if (!conn) {
        return;
    }

    std::unique_lock<std::mutex> lock(mutex_);

    stats_.releases++;
    stats_.active_connections--;

    // Check if connection should be closed
    if (shouldClose(conn)) {
        lock.unlock();
        closeConnection(conn);
        lock.lock();
        available_.notify_one();
        return;
    }

    // Reset connection state based on pool mode
    if (effective_config_.mode == PoolMode::STATEMENT ||
        effective_config_.mode == PoolMode::TRANSACTION) {
        lock.unlock();
        resetConnection(conn);
        lock.lock();
    }

    // Return to idle pool
    conn->setState(ConnectionState::IDLE);
    conn->updateIdleSince();
    idle_connections_.push_back(conn);
    stats_.idle_connections++;

    available_.notify_one();
}

core::Status DatabasePool::prewarm(uint32_t count, core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);

    uint32_t created = 0;
    for (uint32_t i = 0; i < count && all_connections_.size() < effective_config_.max_connections; ++i) {
        auto conn = std::make_unique<PooledConnection>();
        ConnectionConfig config;
        config.database = database_name_;

        if (conn->connect(config, ctx) == core::Status::OK) {
            conn->setPool(this);
            conn->setState(ConnectionState::IDLE);
            conn->updateIdleSince();

            auto* raw_conn = conn.get();
            all_connections_.push_back(std::move(conn));
            idle_connections_.push_back(raw_conn);

            stats_.total_connections++;
            stats_.idle_connections++;
            stats_.creates++;
            created++;
        }
    }

    if (created < count && ctx) {
        ctx->message = "Only created " + std::to_string(created) + " of " +
                       std::to_string(count) + " connections";
        ctx->code = core::Status::OK;  // Still a partial success
    }

    return core::Status::OK;
}

void DatabasePool::drain() {
    draining_ = true;
}

void DatabasePool::resume() {
    draining_ = false;
}

void DatabasePool::forceCloseAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    shutdown_ = true;

    for (auto& conn : all_connections_) {
        conn->close();
    }

    all_connections_.clear();
    idle_connections_.clear();

    stats_.total_connections = 0;
    stats_.active_connections = 0;
    stats_.idle_connections = 0;
}

bool DatabasePool::waitForIdle(std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    std::unique_lock<std::mutex> lock(mutex_);

    while (stats_.active_connections.load() > 0) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        available_.wait_until(lock, deadline);
    }

    return true;
}

void DatabasePool::removeConnection(PooledConnection* conn) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Remove from idle list if present
    idle_connections_.erase(
        std::remove(idle_connections_.begin(), idle_connections_.end(), conn),
        idle_connections_.end());

    // Remove from all_connections
    auto it = std::find_if(all_connections_.begin(), all_connections_.end(),
        [conn](const auto& ptr) { return ptr.get() == conn; });

    if (it != all_connections_.end()) {
        (*it)->close();
        all_connections_.erase(it);
        stats_.total_connections--;
        stats_.closes++;
    }
}

void DatabasePool::markConnectionBroken(PooledConnection* conn) {
    conn->markBroken();
    removeConnection(conn);
}

void DatabasePool::clearStatementCache() {
    // TODO: Implement statement cache clearing
}

void DatabasePool::clearResultCache() {
    // TODO: Implement result cache clearing
}

void DatabasePool::invalidateCacheForTable(const std::string& table_name) {
    // TODO: Implement table-specific cache invalidation
}

std::vector<PooledConnection*> DatabasePool::getIdleConnections() {
    std::lock_guard<std::mutex> lock(mutex_);
    return idle_connections_;
}

std::vector<PooledConnection*> DatabasePool::getAllConnections() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<PooledConnection*> result;
    result.reserve(all_connections_.size());
    for (auto& conn : all_connections_) {
        result.push_back(conn.get());
    }
    return result;
}

void DatabasePool::updateConfig(const DatabasePoolConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (config.max_connections > 0) {
        effective_config_.max_connections = config.max_connections;
    }
    if (config.min_idle > 0) {
        effective_config_.min_idle = config.min_idle;
    }
    if (config.max_idle > 0) {
        effective_config_.max_idle = config.max_idle;
    }
    if (config.acquire_timeout_ms > 0) {
        effective_config_.acquire_timeout_ms = config.acquire_timeout_ms;
    }
    if (config.idle_timeout_sec > 0) {
        effective_config_.idle_timeout_sec = config.idle_timeout_sec;
    }
}

PooledConnection* DatabasePool::tryGetIdleConnection(const std::string& user) {
    if (idle_connections_.empty()) {
        return nullptr;
    }

    // Find a connection for the user
    for (auto it = idle_connections_.begin(); it != idle_connections_.end(); ++it) {
        auto* conn = *it;
        if (conn->user() == user || user.empty()) {
            // Validate if needed
            if (effective_config_.validate_on_acquire) {
                auto now = std::chrono::steady_clock::now();
                auto since_validation = std::chrono::duration_cast<std::chrono::seconds>(
                    now - conn->lastValidated());

                if (since_validation.count() > static_cast<int64_t>(effective_config_.validation_interval_sec)) {
                    if (!conn->validate(effective_config_.validation_query,
                                       effective_config_.validation_timeout_ms)) {
                        conn->incrementValidationFailures();
                        stats_.health_check.validation_failures++;

                        if (conn->validationFailures() >= effective_config_.max_validation_failures) {
                            markConnectionBroken(conn);
                            continue;
                        }
                    }
                }
            }

            idle_connections_.erase(it);
            return conn;
        }
    }

    return nullptr;
}

PooledConnection* DatabasePool::tryGetConnectionWithTags(
    const std::string& user,
    const std::map<std::string, std::string>& tags) {

    for (auto it = idle_connections_.begin(); it != idle_connections_.end(); ++it) {
        auto* conn = *it;
        if (conn->user() == user || user.empty()) {
            // Check tags
            bool match = true;
            for (const auto& pair : tags) {
                if (conn->getTag(pair.first) != pair.second) {
                    match = false;
                    break;
                }
            }

            if (match) {
                idle_connections_.erase(it);
                return conn;
            }
        }
    }

    return nullptr;
}

PooledConnection* DatabasePool::tryGetConnectionWithAffinity(const std::string& client_id) {
    for (auto it = idle_connections_.begin(); it != idle_connections_.end(); ++it) {
        auto* conn = *it;
        if (conn->affinity() == client_id) {
            idle_connections_.erase(it);
            return conn;
        }
    }

    return nullptr;
}

PooledConnection* DatabasePool::createConnection(const std::string& user, core::ErrorContext* ctx) {
    auto conn = std::make_unique<PooledConnection>();

    ConnectionConfig config;
    config.database = database_name_;
    config.user = user;
    config.connect_timeout_ms = effective_config_.connect_timeout_ms;

    if (conn->connect(config, ctx) != core::Status::OK) {
        return nullptr;
    }

    conn->setPool(this);
    conn->setState(ConnectionState::ACQUIRED);

    auto* raw_conn = conn.get();

    std::lock_guard<std::mutex> lock(mutex_);
    all_connections_.push_back(std::move(conn));

    return raw_conn;
}

bool DatabasePool::shouldClose(PooledConnection* conn) {
    if (conn->isBroken() || !conn->isValid()) {
        return true;
    }

    // Check max lifetime
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - conn->createdAt());
    if (age.count() > static_cast<int64_t>(effective_config_.max_lifetime_sec)) {
        return true;
    }

    // Check if over max idle limit
    if (idle_connections_.size() >= effective_config_.max_idle) {
        return true;
    }

    return false;
}

void DatabasePool::closeConnection(PooledConnection* conn) {
    conn->close();
    removeConnection(conn);
}

void DatabasePool::resetConnection(PooledConnection* conn) {
    if (conn->inTransaction()) {
        // TODO: Rollback transaction
        conn->setInTransaction(false);
    }

    if (conn->needsReset()) {
        conn->fullReset();
    } else {
        conn->fastReset();
    }
}

// =============================================================================
// PoolManager Implementation
// =============================================================================

PoolManager& PoolManager::instance() {
    static PoolManager instance;
    return instance;
}

PoolManager::~PoolManager() {
    if (initialized_) {
        shutdown();
    }
}

core::Status PoolManager::initialize(const PoolConfig& config, core::ErrorContext* ctx) {
    if (initialized_) {
        return core::Status::OK;
    }

    global_config_ = config;
    shutdown_flag_ = false;
    initialized_ = true;

    if (config.stats_enabled) {
        startEvictor();
    }

    return core::Status::OK;
}

core::Status PoolManager::shutdown(std::chrono::seconds timeout, core::ErrorContext* ctx) {
    if (!initialized_) {
        return core::Status::OK;
    }

    shutdown_flag_ = true;

    stopHealthChecker();
    stopEvictor();

    // Wait for pools to drain
    auto deadline = std::chrono::steady_clock::now() + timeout;

    {
        std::shared_lock<std::shared_mutex> lock(pools_mutex_);
        for (auto& pair : pools_) {
            pair.second->drain();
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now());
            if (remaining.count() > 0) {
                pair.second->waitForIdle(remaining);
            }
        }
    }

    // Force close all
    {
        std::unique_lock<std::shared_mutex> lock(pools_mutex_);
        for (auto& pair : pools_) {
            pair.second->forceCloseAll();
        }
        pools_.clear();
    }

    initialized_ = false;
    return core::Status::OK;
}

DatabasePool* PoolManager::getPool(const std::string& database) {
    std::shared_lock<std::shared_mutex> lock(pools_mutex_);

    auto it = pools_.find(database);
    if (it != pools_.end()) {
        return it->second.get();
    }
    return nullptr;
}

DatabasePool* PoolManager::getOrCreatePool(const std::string& database,
                                            const DatabasePoolConfig& config) {
    {
        std::shared_lock<std::shared_mutex> lock(pools_mutex_);
        auto it = pools_.find(database);
        if (it != pools_.end()) {
            return it->second.get();
        }
    }

    std::unique_lock<std::shared_mutex> lock(pools_mutex_);

    // Double-check after acquiring write lock
    auto it = pools_.find(database);
    if (it != pools_.end()) {
        return it->second.get();
    }

    DatabasePoolConfig effective_config = config;
    auto db_config_it = db_configs_.find(database);
    if (db_config_it != db_configs_.end()) {
        effective_config = db_config_it->second;
    }

    auto pool = std::make_unique<DatabasePool>(database, global_config_, effective_config);

    // Pre-warm if configured
    if (global_config_.prewarm) {
        pool->prewarm(global_config_.prewarm_count);
    }

    auto* raw_pool = pool.get();
    pools_[database] = std::move(pool);

    return raw_pool;
}

std::vector<std::string> PoolManager::poolNames() const {
    std::shared_lock<std::shared_mutex> lock(pools_mutex_);

    std::vector<std::string> names;
    names.reserve(pools_.size());
    for (const auto& pair : pools_) {
        names.push_back(pair.first);
    }
    return names;
}

PooledConnection* PoolManager::acquire(const std::string& database,
                                        const std::string& user,
                                        std::chrono::milliseconds timeout,
                                        core::ErrorContext* ctx) {
    auto* pool = getOrCreatePool(database);
    if (!pool) {
        SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR,
                         "Failed to get or create pool for database");
        return nullptr;
    }

    return pool->acquire(user, timeout, ctx);
}

void PoolManager::release(PooledConnection* conn) {
    if (!conn) {
        return;
    }

    auto* pool = conn->pool();
    if (pool) {
        pool->release(conn);
    }
}

void PoolManager::setDatabaseConfig(const std::string& database, const DatabasePoolConfig& config) {
    std::unique_lock<std::shared_mutex> lock(pools_mutex_);
    db_configs_[database] = config;

    // Update existing pool if present
    auto it = pools_.find(database);
    if (it != pools_.end()) {
        it->second->updateConfig(config);
    }
}

void PoolManager::clearAllStatementCaches() {
    std::shared_lock<std::shared_mutex> lock(pools_mutex_);
    for (auto& pair : pools_) {
        pair.second->clearStatementCache();
    }
}

void PoolManager::clearAllResultCaches() {
    std::shared_lock<std::shared_mutex> lock(pools_mutex_);
    for (auto& pair : pools_) {
        pair.second->clearResultCache();
    }
}

void PoolManager::invalidateTableInAllPools(const std::string& table_name) {
    std::shared_lock<std::shared_mutex> lock(pools_mutex_);
    for (auto& pair : pools_) {
        pair.second->invalidateCacheForTable(table_name);
    }
}

PoolStatisticsSnapshot PoolManager::aggregateStatistics() const {
    PoolStatisticsSnapshot aggregate{};
    aggregate.start_time = std::chrono::steady_clock::now();

    std::shared_lock<std::shared_mutex> lock(pools_mutex_);
    for (const auto& pair : pools_) {
        const auto& stats = pair.second->statistics();

        aggregate.total_connections += stats.total_connections.load();
        aggregate.active_connections += stats.active_connections.load();
        aggregate.idle_connections += stats.idle_connections.load();
        aggregate.pending_requests += stats.pending_requests.load();
        aggregate.acquires += stats.acquires.load();
        aggregate.releases += stats.releases.load();
        aggregate.creates += stats.creates.load();
        aggregate.closes += stats.closes.load();
        aggregate.timeouts += stats.timeouts.load();
        aggregate.waits += stats.waits.load();
        aggregate.total_acquire_time_us += stats.total_acquire_time_us.load();
        aggregate.total_wait_time_us += stats.total_wait_time_us.load();

        // Take max of max times
        uint64_t max_acquire = stats.max_acquire_time_us.load();
        if (max_acquire > aggregate.max_acquire_time_us) {
            aggregate.max_acquire_time_us = max_acquire;
        }

        uint64_t max_wait = stats.max_wait_time_us.load();
        if (max_wait > aggregate.max_wait_time_us) {
            aggregate.max_wait_time_us = max_wait;
        }

        // Aggregate cache statistics
        aggregate.stmt_cache.hits += stats.stmt_cache.hits.load();
        aggregate.stmt_cache.misses += stats.stmt_cache.misses.load();
        aggregate.stmt_cache.inserts += stats.stmt_cache.inserts.load();
        aggregate.stmt_cache.evictions += stats.stmt_cache.evictions.load();
        aggregate.stmt_cache.invalidations += stats.stmt_cache.invalidations.load();
        aggregate.stmt_cache.current_size += stats.stmt_cache.current_size.load();
        aggregate.stmt_cache.current_memory += stats.stmt_cache.current_memory.load();

        aggregate.result_cache.hits += stats.result_cache.hits.load();
        aggregate.result_cache.misses += stats.result_cache.misses.load();
        aggregate.result_cache.stale_hits += stats.result_cache.stale_hits.load();
        aggregate.result_cache.inserts += stats.result_cache.inserts.load();
        aggregate.result_cache.evictions += stats.result_cache.evictions.load();
        aggregate.result_cache.invalidations += stats.result_cache.invalidations.load();
        aggregate.result_cache.too_large += stats.result_cache.too_large.load();
        aggregate.result_cache.clears += stats.result_cache.clears.load();
        aggregate.result_cache.current_size += stats.result_cache.current_size.load();
        aggregate.result_cache.current_memory += stats.result_cache.current_memory.load();

        aggregate.health_check.validations += stats.health_check.validations.load();
        aggregate.health_check.validation_failures += stats.health_check.validation_failures.load();
        aggregate.health_check.removed += stats.health_check.removed.load();
        aggregate.health_check.recovered += stats.health_check.recovered.load();
    }

    return aggregate;
}

std::map<std::string, PoolStatisticsSnapshot> PoolManager::allPoolStatistics() const {
    std::map<std::string, PoolStatisticsSnapshot> result;

    std::shared_lock<std::shared_mutex> lock(pools_mutex_);
    for (const auto& pair : pools_) {
        result[pair.first] = PoolStatisticsSnapshot::from(pair.second->statistics());
    }

    return result;
}

void PoolManager::startHealthChecker() {
    if (health_checker_thread_) {
        return;
    }

    health_checker_thread_ = std::make_unique<std::thread>([this]() {
        healthCheckLoop();
    });
}

void PoolManager::stopHealthChecker() {
    if (!health_checker_thread_) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(health_mutex_);
        shutdown_flag_ = true;
    }
    health_cv_.notify_all();

    if (health_checker_thread_->joinable()) {
        health_checker_thread_->join();
    }
    health_checker_thread_.reset();
}

void PoolManager::startEvictor() {
    if (evictor_thread_) {
        return;
    }

    evictor_thread_ = std::make_unique<std::thread>([this]() {
        evictionLoop();
    });
}

void PoolManager::stopEvictor() {
    if (!evictor_thread_) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(evictor_mutex_);
        shutdown_flag_ = true;
    }
    evictor_cv_.notify_all();

    if (evictor_thread_->joinable()) {
        evictor_thread_->join();
    }
    evictor_thread_.reset();
}

void PoolManager::logPoolStatus() {
    // TODO: Log to system log
}

std::string PoolManager::getPoolStatusJson() const {
    std::ostringstream ss;
    ss << "{";
    ss << "\"total_pools\":" << pools_.size() << ",";

    auto stats = aggregateStatistics();
    ss << "\"total_connections\":" << stats.total_connections << ",";
    ss << "\"active_connections\":" << stats.active_connections << ",";
    ss << "\"idle_connections\":" << stats.idle_connections << ",";
    ss << "\"acquires\":" << stats.acquires << ",";
    ss << "\"releases\":" << stats.releases << ",";
    ss << "\"timeouts\":" << stats.timeouts;
    ss << "}";

    return ss.str();
}

void PoolManager::healthCheckLoop() {
    while (!shutdown_flag_) {
        std::unique_lock<std::mutex> lock(health_mutex_);
        health_cv_.wait_for(lock, std::chrono::seconds(global_config_.validation_interval_sec),
            [this]() { return shutdown_flag_.load(); });

        if (shutdown_flag_) {
            break;
        }

        // Validate connections in all pools
        std::shared_lock<std::shared_mutex> pools_lock(pools_mutex_);
        for (auto& pair : pools_) {
            auto idle_conns = pair.second->getIdleConnections();
            for (auto* conn : idle_conns) {
                if (!conn->validate(global_config_.validation_query,
                                   global_config_.validation_timeout_ms)) {
                    pair.second->markConnectionBroken(conn);
                }
            }
        }
    }
}

void PoolManager::evictionLoop() {
    while (!shutdown_flag_) {
        std::unique_lock<std::mutex> lock(evictor_mutex_);
        evictor_cv_.wait_for(lock, std::chrono::seconds(global_config_.idle_timeout_sec / 2),
            [this]() { return shutdown_flag_.load(); });

        if (shutdown_flag_) {
            break;
        }

        // Evict idle connections
        std::shared_lock<std::shared_mutex> pools_lock(pools_mutex_);
        for (auto& pair : pools_) {
            auto idle_conns = pair.second->getIdleConnections();
            auto now = std::chrono::steady_clock::now();

            for (auto* conn : idle_conns) {
                auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(
                    now - conn->idleSince());

                if (idle_time.count() > static_cast<int64_t>(global_config_.idle_timeout_sec)) {
                    // Only remove if above min_idle
                    if (pair.second->idleConnectionCount() > pair.second->minIdle()) {
                        pair.second->removeConnection(conn);
                    }
                }
            }
        }
    }
}

void PoolManager::statsLoop() {
    while (!shutdown_flag_) {
        std::this_thread::sleep_for(std::chrono::seconds(global_config_.stats_interval_sec));

        if (shutdown_flag_) {
            break;
        }

        if (global_config_.log_pool_stats) {
            logPoolStatus();
        }
    }
}

// =============================================================================
// Utility Functions
// =============================================================================

const char* poolModeToString(PoolMode mode) {
    switch (mode) {
        case PoolMode::SESSION: return "SESSION";
        case PoolMode::TRANSACTION: return "TRANSACTION";
        case PoolMode::STATEMENT: return "STATEMENT";
        default: return "UNKNOWN";
    }
}

bool parsePoolMode(const std::string& str, PoolMode& mode) {
    if (str == "SESSION" || str == "session") {
        mode = PoolMode::SESSION;
        return true;
    }
    if (str == "TRANSACTION" || str == "transaction") {
        mode = PoolMode::TRANSACTION;
        return true;
    }
    if (str == "STATEMENT" || str == "statement") {
        mode = PoolMode::STATEMENT;
        return true;
    }
    return false;
}

const char* connectionStateToString(ConnectionState state) {
    switch (state) {
        case ConnectionState::CREATED: return "CREATED";
        case ConnectionState::IDLE: return "IDLE";
        case ConnectionState::ACQUIRED: return "ACQUIRED";
        case ConnectionState::IN_TRANSACTION: return "IN_TRANSACTION";
        case ConnectionState::CLOSING: return "CLOSING";
        case ConnectionState::CLOSED: return "CLOSED";
        default: return "UNKNOWN";
    }
}

const char* evictionPolicyToString(EvictionPolicy policy) {
    switch (policy) {
        case EvictionPolicy::LRU: return "LRU";
        case EvictionPolicy::LFU: return "LFU";
        case EvictionPolicy::FIFO: return "FIFO";
        case EvictionPolicy::TTL: return "TTL";
        default: return "UNKNOWN";
    }
}

bool parseEvictionPolicy(const std::string& str, EvictionPolicy& policy) {
    if (str == "LRU" || str == "lru") {
        policy = EvictionPolicy::LRU;
        return true;
    }
    if (str == "LFU" || str == "lfu") {
        policy = EvictionPolicy::LFU;
        return true;
    }
    if (str == "FIFO" || str == "fifo") {
        policy = EvictionPolicy::FIFO;
        return true;
    }
    if (str == "TTL" || str == "ttl") {
        policy = EvictionPolicy::TTL;
        return true;
    }
    return false;
}

// =============================================================================
// PooledConnectionGuard Implementation
// =============================================================================

PooledConnectionGuard::PooledConnectionGuard(PoolManager& manager,
                                              const std::string& database,
                                              const std::string& user,
                                              std::chrono::milliseconds timeout)
    : manager_(&manager)
    , pool_(nullptr) {
    conn_ = manager.acquire(database, user, timeout);
    if (conn_) {
        pool_ = conn_->pool();
    }
}

PooledConnectionGuard::PooledConnectionGuard(DatabasePool& pool,
                                              const std::string& user,
                                              std::chrono::milliseconds timeout)
    : pool_(&pool)
    , manager_(nullptr) {
    conn_ = pool.acquire(user, timeout);
}

PooledConnectionGuard::~PooledConnectionGuard() {
    if (conn_) {
        if (pool_) {
            pool_->release(conn_);
        } else if (manager_) {
            manager_->release(conn_);
        }
    }
}

PooledConnectionGuard::PooledConnectionGuard(PooledConnectionGuard&& other) noexcept
    : conn_(other.conn_)
    , pool_(other.pool_)
    , manager_(other.manager_) {
    other.conn_ = nullptr;
    other.pool_ = nullptr;
    other.manager_ = nullptr;
}

PooledConnectionGuard& PooledConnectionGuard::operator=(PooledConnectionGuard&& other) noexcept {
    if (this != &other) {
        if (conn_) {
            if (pool_) {
                pool_->release(conn_);
            } else if (manager_) {
                manager_->release(conn_);
            }
        }

        conn_ = other.conn_;
        pool_ = other.pool_;
        manager_ = other.manager_;

        other.conn_ = nullptr;
        other.pool_ = nullptr;
        other.manager_ = nullptr;
    }
    return *this;
}

PooledConnection* PooledConnectionGuard::release() {
    auto* conn = conn_;
    conn_ = nullptr;
    pool_ = nullptr;
    manager_ = nullptr;
    return conn;
}

}  // namespace pool
}  // namespace scratchbird

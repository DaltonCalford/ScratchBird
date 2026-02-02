/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// P2-22: Connection Pool Implementation
//
// Thread-safe connection pool for managing database connections efficiently.
//
// November 25, 2025

#include "scratchbird/core/connection_pool.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/logger.h"

namespace scratchbird::core {

// =================================================================================================
// PooledConnection Implementation
// =================================================================================================

PooledConnection::PooledConnection(ConnectionContext* conn, ConnectionPool* pool)
    : conn_(conn), pool_(pool), invalidated_(false)
{
}

PooledConnection::~PooledConnection()
{
    release();
}

PooledConnection::PooledConnection(PooledConnection&& other) noexcept
    : conn_(other.conn_), pool_(other.pool_), invalidated_(other.invalidated_)
{
    other.conn_ = nullptr;
    other.pool_ = nullptr;
}

PooledConnection& PooledConnection::operator=(PooledConnection&& other) noexcept
{
    if (this != &other) {
        release();
        conn_ = other.conn_;
        pool_ = other.pool_;
        invalidated_ = other.invalidated_;
        other.conn_ = nullptr;
        other.pool_ = nullptr;
    }
    return *this;
}

void PooledConnection::release()
{
    if (conn_ && pool_) {
        pool_->release(conn_, invalidated_);
        conn_ = nullptr;
        pool_ = nullptr;
    }
}

void PooledConnection::invalidate()
{
    invalidated_ = true;
}

// =================================================================================================
// ConnectionPool Implementation
// =================================================================================================

ConnectionPool::ConnectionPool(Database* db, const ConnectionPoolConfig& config)
    : db_(db), config_(config)
{
    // Initialize statistics
    stats_.total_connections_created = 0;
    stats_.total_connections_destroyed = 0;
    stats_.total_acquires = 0;
    stats_.total_releases = 0;
    stats_.acquire_timeouts = 0;
    stats_.validation_failures = 0;
    stats_.current_pool_size = 0;
    stats_.current_in_use = 0;
    stats_.current_idle = 0;
    stats_.peak_in_use = 0;

    // Start cleanup thread
    cleanup_running_ = true;
    cleanup_thread_ = std::thread(&ConnectionPool::runCleanup, this);

    // Warm up the pool with minimum connections
    warmup();

    LOG_INFO(GENERAL, "Connection pool '%s' initialized (min=%zu, max=%zu)",
             config_.pool_name.c_str(), config_.min_connections, config_.max_connections);
}

ConnectionPool::~ConnectionPool()
{
    shutdown();
}

PooledConnection ConnectionPool::acquire(ErrorContext* ctx)
{
    std::unique_lock<std::mutex> lock(mutex_);

    auto deadline = std::chrono::steady_clock::now() + config_.acquire_timeout;

    while (true) {
        if (shutdown_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Connection pool is shut down");
            return PooledConnection();
        }

        // Try to get an idle connection
        while (!idle_connections_.empty()) {
            auto* info = idle_connections_.front();
            idle_connections_.pop();

            // Check connection lifetime
            auto now = std::chrono::steady_clock::now();
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - info->created_at);
            if (age > config_.max_lifetime) {
                // Connection too old, destroy it
                destroyConnection(info->conn.get());
                continue;
            }

            // Validate if configured
            if (config_.validate_on_acquire && !validateConnection(info->conn.get())) {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                ++stats_.validation_failures;
                destroyConnection(info->conn.get());
                continue;
            }

            // Mark as in use
            info->in_use = true;
            info->last_used = now;
            ++info->use_count;

            // Update stats
            {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                ++stats_.total_acquires;
                ++stats_.current_in_use;
                --stats_.current_idle;
                stats_.last_acquire_time = now;
                if (stats_.current_in_use > stats_.peak_in_use) {
                    stats_.peak_in_use = stats_.current_in_use;
                }
            }

            return PooledConnection(info->conn.get(), this);
        }

        // No idle connections available
        // Try to create a new one if below max
        if (all_connections_.size() < config_.max_connections) {
            lock.unlock();
            ConnectionContext* new_conn = createConnection(ctx);
            lock.lock();

            if (new_conn) {
                // Add to tracking
                PooledConnectionInfo info;
                info.conn.reset(new_conn);
                info.created_at = std::chrono::steady_clock::now();
                info.last_used = info.created_at;
                info.use_count = 1;
                info.in_use = true;

                auto [it, inserted] = all_connections_.emplace(new_conn, std::move(info));

                // Update stats
                {
                    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                    ++stats_.total_connections_created;
                    ++stats_.total_acquires;
                    ++stats_.current_pool_size;
                    ++stats_.current_in_use;
                    stats_.last_acquire_time = it->second.created_at;
                    if (stats_.current_in_use > stats_.peak_in_use) {
                        stats_.peak_in_use = stats_.current_in_use;
                    }
                }

                return PooledConnection(new_conn, this);
            }
        }

        // Wait for a connection to become available
        if (available_.wait_until(lock, deadline) == std::cv_status::timeout) {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            ++stats_.acquire_timeouts;
            SET_ERROR_CONTEXT(ctx, Status::LOCK_TIMEOUT, "Timeout acquiring connection from pool");
            return PooledConnection();
        }
    }
}

PooledConnection ConnectionPool::tryAcquire()
{
    std::unique_lock<std::mutex> lock(mutex_);

    if (shutdown_ || idle_connections_.empty()) {
        return PooledConnection();
    }

    auto* info = idle_connections_.front();
    idle_connections_.pop();

    // Mark as in use
    info->in_use = true;
    info->last_used = std::chrono::steady_clock::now();
    ++info->use_count;

    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        ++stats_.total_acquires;
        ++stats_.current_in_use;
        --stats_.current_idle;
        stats_.last_acquire_time = info->last_used;
        if (stats_.current_in_use > stats_.peak_in_use) {
            stats_.peak_in_use = stats_.current_in_use;
        }
    }

    return PooledConnection(info->conn.get(), this);
}

void ConnectionPool::release(ConnectionContext* conn, bool invalidated)
{
    if (!conn) return;

    std::unique_lock<std::mutex> lock(mutex_);

    auto it = all_connections_.find(conn);
    if (it == all_connections_.end()) {
        // Connection not from this pool
        LOG_ERROR(GENERAL, "Connection not from pool returned");
        return;
    }

    auto& info = it->second;

    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        ++stats_.total_releases;
        --stats_.current_in_use;
        stats_.last_release_time = std::chrono::steady_clock::now();
    }

    if (invalidated || shutdown_) {
        // Destroy the connection
        destroyConnection(conn);
        return;
    }

    // Validate on return if configured
    if (config_.validate_on_return && !validateConnection(conn)) {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        ++stats_.validation_failures;
        destroyConnection(conn);
        return;
    }

    // Return to idle pool
    info.in_use = false;
    info.last_used = std::chrono::steady_clock::now();
    idle_connections_.push(&info);

    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        ++stats_.current_idle;
    }

    // Notify waiters
    lock.unlock();
    available_.notify_one();
}

void ConnectionPool::shutdown()
{
    // Stop cleanup thread
    cleanup_running_ = false;
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }

    std::unique_lock<std::mutex> lock(mutex_);
    shutdown_ = true;

    // Close all idle connections
    while (!idle_connections_.empty()) {
        auto* info = idle_connections_.front();
        idle_connections_.pop();
        destroyConnection(info->conn.get());
    }

    // Note: In-use connections will be destroyed when returned

    LOG_INFO(GENERAL, "Connection pool '%s' shut down", config_.pool_name.c_str());

    // Notify any waiters
    lock.unlock();
    available_.notify_all();
}

void ConnectionPool::resize(size_t new_max)
{
    std::lock_guard<std::mutex> lock(mutex_);
    config_.max_connections = new_max;

    // If we have more connections than the new max, drain excess idle connections
    while (!idle_connections_.empty() && all_connections_.size() > new_max) {
        auto* info = idle_connections_.front();
        idle_connections_.pop();
        destroyConnection(info->conn.get());
    }
}

void ConnectionPool::drain()
{
    std::lock_guard<std::mutex> lock(mutex_);

    while (!idle_connections_.empty()) {
        auto* info = idle_connections_.front();
        idle_connections_.pop();
        destroyConnection(info->conn.get());
    }

    LOG_INFO(GENERAL, "Connection pool '%s' drained", config_.pool_name.c_str());
}

void ConnectionPool::warmup()
{
    std::lock_guard<std::mutex> lock(mutex_);

    while (all_connections_.size() < config_.min_connections) {
        ConnectionContext* new_conn = createConnection(nullptr);
        if (!new_conn) break;

        PooledConnectionInfo info;
        info.conn.reset(new_conn);
        info.created_at = std::chrono::steady_clock::now();
        info.last_used = info.created_at;
        info.use_count = 0;
        info.in_use = false;

        auto [it, inserted] = all_connections_.emplace(new_conn, std::move(info));
        idle_connections_.push(&it->second);

        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            ++stats_.total_connections_created;
            ++stats_.current_pool_size;
            ++stats_.current_idle;
        }
    }
}

ConnectionPool::Statistics ConnectionPool::getStatistics() const
{
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void ConnectionPool::resetStatistics()
{
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.total_acquires = 0;
    stats_.total_releases = 0;
    stats_.acquire_timeouts = 0;
    stats_.validation_failures = 0;
    stats_.peak_in_use = stats_.current_in_use;
}

ConnectionContext* ConnectionPool::createConnection(ErrorContext* ctx)
{
    // Get next proc_id
    static std::atomic<uint32_t> proc_id_counter{1};
    uint32_t proc_id = proc_id_counter.fetch_add(1);

    auto* conn = new ConnectionContext(db_, proc_id);
    auto status = conn->initialize(ctx);

    if (status != Status::OK) {
        delete conn;
        return nullptr;
    }

    return conn;
}

void ConnectionPool::destroyConnection(ConnectionContext* conn)
{
    auto it = all_connections_.find(conn);
    if (it != all_connections_.end()) {
        all_connections_.erase(it);

        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        ++stats_.total_connections_destroyed;
        --stats_.current_pool_size;
    }
}

bool ConnectionPool::validateConnection(ConnectionContext* conn)
{
    // Basic validation: check if connection is still valid
    // Could be extended to run a simple query like "SELECT 1"
    if (!conn) return false;

    // Check if transaction is still active and not terminated
    ErrorContext ctx;
    auto status = conn->checkTerminationRequested(&ctx);
    return status == Status::OK;
}

void ConnectionPool::runCleanup()
{
    while (cleanup_running_) {
        // Sleep for a portion of the idle timeout
        std::this_thread::sleep_for(config_.idle_timeout / 4);

        if (!cleanup_running_) break;

        cleanupIdleConnections();
    }
}

void ConnectionPool::cleanupIdleConnections()
{
    std::unique_lock<std::mutex> lock(mutex_);

    if (shutdown_) return;

    auto now = std::chrono::steady_clock::now();
    std::vector<PooledConnectionInfo*> to_close;

    // Find idle connections that have timed out
    // But keep at least min_connections
    size_t idle_count = idle_connections_.size();
    std::queue<PooledConnectionInfo*> kept;

    while (!idle_connections_.empty()) {
        auto* info = idle_connections_.front();
        idle_connections_.pop();

        auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(now - info->last_used);

        // Check if connection is too old or idle too long
        // But always keep min_connections
        if (idle_count > config_.min_connections &&
            (idle_time > config_.idle_timeout ||
             std::chrono::duration_cast<std::chrono::seconds>(now - info->created_at) > config_.max_lifetime))
        {
            to_close.push_back(info);
            --idle_count;
        }
        else
        {
            kept.push(info);
        }
    }

    idle_connections_ = std::move(kept);

    // Close connections outside of lock
    lock.unlock();

    for (auto* info : to_close) {
        lock.lock();
        destroyConnection(info->conn.get());
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            --stats_.current_idle;
        }
        lock.unlock();
    }

    if (!to_close.empty()) {
        LOG_DEBUG(GENERAL, "Connection pool '%s': cleaned up %zu idle connections",
                  config_.pool_name.c_str(), to_close.size());
    }
}

// =================================================================================================
// ConnectionPoolManager Implementation
// =================================================================================================

ConnectionPoolManager& ConnectionPoolManager::getInstance()
{
    static ConnectionPoolManager instance;
    return instance;
}

ConnectionPoolManager::~ConnectionPoolManager()
{
    shutdownAll();
}

ConnectionPool* ConnectionPoolManager::getPool(Database* db, const std::string& pool_name)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = pools_.find(pool_name);
    if (it != pools_.end()) {
        return it->second.get();
    }

    // Create default pool
    auto pool = std::make_unique<ConnectionPool>(db);
    auto* ptr = pool.get();
    pools_[pool_name] = std::move(pool);
    return ptr;
}

ConnectionPool* ConnectionPoolManager::createPool(Database* db, const std::string& pool_name,
                                                   const ConnectionPoolConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Remove existing pool if any
    pools_.erase(pool_name);

    auto pool = std::make_unique<ConnectionPool>(db, config);
    auto* ptr = pool.get();
    pools_[pool_name] = std::move(pool);
    return ptr;
}

void ConnectionPoolManager::removePool(const std::string& pool_name)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = pools_.find(pool_name);
    if (it != pools_.end()) {
        it->second->shutdown();
        pools_.erase(it);
    }
}

void ConnectionPoolManager::shutdownAll()
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& [name, pool] : pools_) {
        pool->shutdown();
    }
    pools_.clear();
}

} // namespace scratchbird::core

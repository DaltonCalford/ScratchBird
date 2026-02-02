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
// P2-22: Connection Pool
//
// Thread-safe connection pool for managing database connections efficiently.
// Features:
// - Configurable pool size (min/max connections)
// - Connection reuse with validation
// - Idle timeout for connection cleanup
// - Statistics and monitoring
//
// November 25, 2025

#pragma once

#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <thread>

namespace scratchbird::core {

// Forward declaration
class Database;

// Connection pool configuration
struct ConnectionPoolConfig {
    size_t min_connections = 5;          // Minimum connections to maintain
    size_t max_connections = 100;        // Maximum connections allowed
    std::chrono::seconds idle_timeout{300};  // Time before idle connections are closed (5 min)
    std::chrono::seconds max_lifetime{3600}; // Max lifetime for a connection (1 hour)
    std::chrono::milliseconds acquire_timeout{30000}; // Max wait time to acquire (30 sec)
    bool validate_on_acquire = true;     // Validate connection before returning
    bool validate_on_return = false;     // Validate connection when returned
    std::string pool_name = "default";   // Name for logging/monitoring
};

// Pooled connection wrapper (RAII - returns connection to pool on destruction)
class PooledConnection {
public:
    PooledConnection() = default;
    PooledConnection(ConnectionContext* conn, class ConnectionPool* pool);
    ~PooledConnection();

    // Move only
    PooledConnection(PooledConnection&& other) noexcept;
    PooledConnection& operator=(PooledConnection&& other) noexcept;
    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;

    // Access the underlying connection
    ConnectionContext* get() const { return conn_; }
    ConnectionContext* operator->() const { return conn_; }
    ConnectionContext& operator*() const { return *conn_; }
    explicit operator bool() const { return conn_ != nullptr; }

    // Release connection back to pool without waiting for destructor
    void release();

    // Invalidate connection (mark as unusable, will be destroyed instead of pooled)
    void invalidate();

private:
    ConnectionContext* conn_ = nullptr;
    ConnectionPool* pool_ = nullptr;
    bool invalidated_ = false;
};

// Connection pool for efficient connection management
class ConnectionPool {
public:
    explicit ConnectionPool(Database* db, const ConnectionPoolConfig& config = {});
    ~ConnectionPool();

    // Disable copy
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    // Acquire a connection from the pool
    // Blocks until a connection is available or timeout expires
    PooledConnection acquire(ErrorContext* ctx = nullptr);

    // Try to acquire without blocking (returns empty PooledConnection if none available)
    PooledConnection tryAcquire();

    // Return a connection to the pool (called by PooledConnection)
    void release(ConnectionContext* conn, bool invalidated);

    // Pool management
    void shutdown();               // Graceful shutdown - close all connections
    void resize(size_t new_max);   // Change max pool size
    void drain();                  // Remove all idle connections
    void warmup();                 // Pre-create min_connections

    // Statistics
    struct Statistics {
        uint64_t total_connections_created;
        uint64_t total_connections_destroyed;
        uint64_t total_acquires;
        uint64_t total_releases;
        uint64_t acquire_timeouts;
        uint64_t validation_failures;
        size_t current_pool_size;
        size_t current_in_use;
        size_t current_idle;
        size_t peak_in_use;
        std::chrono::steady_clock::time_point last_acquire_time;
        std::chrono::steady_clock::time_point last_release_time;
    };

    Statistics getStatistics() const;
    void resetStatistics();

    // Configuration access
    const ConnectionPoolConfig& config() const { return config_; }

private:
    Database* db_;
    ConnectionPoolConfig config_;

    // Connection tracking
    struct PooledConnectionInfo {
        std::unique_ptr<ConnectionContext> conn;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point last_used;
        uint64_t use_count;
        bool in_use;
    };

    // Pool state
    std::queue<PooledConnectionInfo*> idle_connections_;
    std::unordered_map<ConnectionContext*, PooledConnectionInfo> all_connections_;
    std::mutex mutex_;
    std::condition_variable available_;
    bool shutdown_ = false;

    // Background cleanup thread
    std::thread cleanup_thread_;
    std::atomic<bool> cleanup_running_{false};

    // Statistics
    mutable std::mutex stats_mutex_;
    Statistics stats_{};

    // Internal methods
    ConnectionContext* createConnection(ErrorContext* ctx);
    void destroyConnection(ConnectionContext* conn);
    bool validateConnection(ConnectionContext* conn);
    void runCleanup();
    void cleanupIdleConnections();
};

// Global connection pool manager (singleton for multi-database support)
class ConnectionPoolManager {
public:
    static ConnectionPoolManager& getInstance();

    // Get or create pool for a database
    ConnectionPool* getPool(Database* db, const std::string& pool_name = "default");

    // Create a new named pool for a database
    ConnectionPool* createPool(Database* db, const std::string& pool_name,
                               const ConnectionPoolConfig& config);

    // Remove a pool
    void removePool(const std::string& pool_name);

    // Shutdown all pools
    void shutdownAll();

private:
    ConnectionPoolManager() = default;
    ~ConnectionPoolManager();

    std::unordered_map<std::string, std::unique_ptr<ConnectionPool>> pools_;
    std::mutex mutex_;
};

} // namespace scratchbird::core

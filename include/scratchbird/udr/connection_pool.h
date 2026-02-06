/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * UDR Connection Pool
 * 
 * Section D: Remote Engine UDR Connectors
 * 
 * Thread-safe connection pool for UDR connectors with health checking,
 * idle timeout, and metrics collection.
 */

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <thread>
#include <functional>

namespace scratchbird {
namespace udr {

// Forward declarations
class PooledConnection;

// ============================================================================
// Connection Pool Configuration
// ============================================================================

struct ConnectionPoolConfig {
    // Pool sizing
    uint32_t min_size = 1;
    uint32_t max_size = 10;
    uint32_t initial_size = 1;
    
    // Timeouts (milliseconds)
    uint32_t connection_timeout_ms = 10000;
    uint32_t idle_timeout_ms = 300000;  // 5 minutes
    uint32_t max_lifetime_ms = 3600000;  // 1 hour
    uint32_t health_check_interval_ms = 30000;  // 30 seconds
    uint32_t acquire_timeout_ms = 5000;
    
    // Retry settings
    uint32_t max_retries = 3;
    uint32_t retry_delay_ms = 1000;
    
    // Behavior
    bool test_on_borrow = true;
    bool test_on_return = false;
    bool test_while_idle = true;
    bool block_when_exhausted = true;
    
    // Validation query (empty = use connector-specific ping)
    std::string validation_query;
};

// ============================================================================
// Pool Statistics
// ============================================================================

struct ConnectionPoolStats {
    uint32_t total_connections = 0;
    uint32_t active_connections = 0;
    uint32_t idle_connections = 0;
    uint32_t waiting_requests = 0;
    uint64_t total_requests = 0;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t created = 0;
    uint64_t destroyed = 0;
    uint64_t validated = 0;
    uint64_t validation_failures = 0;
    uint64_t wait_time_ms = 0;
    
    double getHitRate() const {
        uint64_t total = hits + misses;
        return total > 0 ? static_cast<double>(hits) / total : 0.0;
    }
    
    uint32_t getTotalConnections() const {
        return total_connections;
    }
};

// ============================================================================
// Connection Factory Interface
// ============================================================================

class ConnectionFactory {
public:
    virtual ~ConnectionFactory() = default;
    
    virtual std::unique_ptr<PooledConnection> createConnection(
        core::ErrorContext* ctx = nullptr) = 0;
    
    virtual bool validateConnection(PooledConnection* conn,
                                    core::ErrorContext* ctx = nullptr) = 0;
    
    virtual void destroyConnection(PooledConnection* conn) = 0;
};

// ============================================================================
// Pooled Connection
// ============================================================================

class PooledConnection {
public:
    PooledConnection();
    virtual ~PooledConnection() = default;
    
    // Non-copyable but movable
    PooledConnection(const PooledConnection&) = delete;
    PooledConnection& operator=(const PooledConnection&) = delete;
    PooledConnection(PooledConnection&&) = default;
    PooledConnection& operator=(PooledConnection&&) = default;

    // Connection state
    virtual bool isOpen() const = 0;
    virtual bool isValid() const = 0;
    virtual core::Status ping(core::ErrorContext* ctx = nullptr) = 0;
    virtual void close() = 0;
    
    // Metadata
    virtual std::string getRemoteAddress() const = 0;
    virtual std::string getRemoteVersion() const = 0;
    
    // Pool tracking (used by ConnectionPool)
    void setPoolId(uint32_t id) { pool_id_ = id; }
    uint32_t getPoolId() const { return pool_id_; }
    
    void markInUse() {
        in_use_ = true;
        last_used_ = std::chrono::steady_clock::now();
        usage_count_++;
    }
    
    void markIdle() {
        in_use_ = false;
        last_used_ = std::chrono::steady_clock::now();
    }
    
    bool isInUse() const { return in_use_; }
    
    std::chrono::steady_clock::time_point getLastUsed() const {
        return last_used_;
    }
    
    std::chrono::steady_clock::time_point getCreated() const {
        return created_;
    }
    
    uint64_t getUsageCount() const { return usage_count_; }
    
    bool isExpired(uint32_t max_lifetime_ms) const;
    bool isIdleTimeout(uint32_t idle_timeout_ms) const;

protected:
    uint32_t pool_id_ = 0;
    std::atomic<bool> in_use_{false};
    std::atomic<uint64_t> usage_count_{0};
    std::chrono::steady_clock::time_point created_;
    std::chrono::steady_clock::time_point last_used_;
};

// ============================================================================
// Connection Pool
// ============================================================================

class ConnectionPool {
public:
    ConnectionPool(const std::string& name,
                   std::unique_ptr<ConnectionFactory> factory,
                   const ConnectionPoolConfig& config);
    ~ConnectionPool();

    // Non-copyable, non-movable
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    core::Status initialize(core::ErrorContext* ctx = nullptr);
    void shutdown();
    bool isRunning() const { return running_; }

    // ========================================================================
    // Connection Management
    // ========================================================================

    std::unique_ptr<PooledConnection> acquire(core::ErrorContext* ctx = nullptr);
    void release(std::unique_ptr<PooledConnection> conn);
    
    // Acquire with timeout
    std::unique_ptr<PooledConnection> acquireWithTimeout(
        uint32_t timeout_ms,
        core::ErrorContext* ctx = nullptr);

    // ========================================================================
    // Pool Operations
    // ========================================================================

    void invalidateAll();
    void evictIdle();
    core::Status ensureMinimumConnections(core::ErrorContext* ctx = nullptr);
    
    // ========================================================================
    // Statistics
    // ========================================================================

    ConnectionPoolStats getStats() const {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        return stats_;
    }
    std::string getName() const { return name_; }
    
    // ========================================================================
    // Configuration
    // ========================================================================

    const ConnectionPoolConfig& getConfig() const { return config_; }
    void setConfig(const ConnectionPoolConfig& config);

private:
    // ========================================================================
    // Internal Methods
    // ========================================================================

    std::unique_ptr<PooledConnection> createConnection(core::ErrorContext* ctx);
    bool validateConnection(PooledConnection* conn, core::ErrorContext* ctx);
    void destroyConnection(std::unique_ptr<PooledConnection> conn);
    
    void returnToPool(std::unique_ptr<PooledConnection> conn);
    void removeFromPool(PooledConnection* conn);
    
    void startMaintenanceThread();
    void stopMaintenanceThread();
    void maintenanceLoop();
    
    void incrementWaiting() { stats_.waiting_requests++; }
    void decrementWaiting() { stats_.waiting_requests--; }

private:
    std::string name_;
    std::unique_ptr<ConnectionFactory> factory_;
    ConnectionPoolConfig config_;
    
    // Pool state
    std::atomic<bool> running_{false};
    std::atomic<uint32_t> next_id_{1};
    
    // Connection storage
    mutable std::mutex pool_mutex_;
    std::condition_variable available_cv_;
    std::vector<std::unique_ptr<PooledConnection>> all_connections_;
    std::queue<PooledConnection*> idle_connections_;
    
    // Maintenance thread
    std::thread maintenance_thread_;
    std::atomic<bool> stop_maintenance_{false};
    
    // Statistics
    ConnectionPoolStats stats_;
};

// ============================================================================
// Scoped Connection Handle
// ============================================================================

class ScopedConnection {
public:
    ScopedConnection(ConnectionPool& pool,
                     std::unique_ptr<PooledConnection> conn);
    ~ScopedConnection();

    // Non-copyable
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;
    
    // Movable
    ScopedConnection(ScopedConnection&& other) noexcept;
    ScopedConnection& operator=(ScopedConnection&& other) noexcept;

    PooledConnection* get() const { return conn_.get(); }
    PooledConnection* operator->() const { return conn_.get(); }
    PooledConnection& operator*() const { return *conn_; }
    
    bool isValid() const { return conn_ != nullptr && conn_->isValid(); }
    
    // Release ownership (caller must return to pool)
    std::unique_ptr<PooledConnection> release();

private:
    ConnectionPool& pool_;
    std::unique_ptr<PooledConnection> conn_;
};

// ============================================================================
// Connection Pool Manager
// ============================================================================

class ConnectionPoolManager {
public:
    static ConnectionPoolManager& instance();
    
    // Pool registration
    void registerPool(const std::string& name,
                      std::unique_ptr<ConnectionPool> pool);
    void unregisterPool(const std::string& name);
    
    // Pool access
    ConnectionPool* getPool(const std::string& name);
    std::vector<std::string> getPoolNames() const;
    
    // Global operations
    void shutdownAll();
    std::vector<std::pair<std::string, ConnectionPoolStats>> getAllStats() const;

private:
    ConnectionPoolManager() = default;
    ~ConnectionPoolManager() = default;
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<ConnectionPool>> pools_;
};

} // namespace udr
} // namespace scratchbird

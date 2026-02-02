/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * @file remote_connection_pool.h
 * @brief Remote Connection Pool for Foreign Data Wrappers
 *
 * Manages pooled connections to remote databases with health monitoring,
 * connection lifecycle management, and statistics collection.
 *
 * Architecture:
 *   RemoteConnectionPoolRegistry (Singleton)
 *   ├── ServerPool (legacy_pg)
 *   │   ├── UserPool (admin) [conn][conn]
 *   │   └── UserPool (readonly) [conn]
 *   └── ServerPool (prod_mysql)
 *       └── UserPool (app_user) [conn][conn]
 *
 * Part of Phase 3.7: UDR Plugin System
 */

#ifndef SCRATCHBIRD_FDW_REMOTE_CONNECTION_POOL_H
#define SCRATCHBIRD_FDW_REMOTE_CONNECTION_POOL_H

#include "scratchbird/fdw/fdw_types.h"
#include "scratchbird/fdw/protocol_adapter.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace scratchbird {
namespace fdw {

// Forward declarations
class RemoteConnectionPoolRegistry;
class ServerPool;
class UserPool;
class PooledRemoteConnection;

// =============================================================================
// UserPool - Per-user connection pool within a server
// =============================================================================

/**
 * @brief Per-user connection pool statistics
 */
struct UserPoolStats {
    std::string local_user;
    uint32_t total_connections = 0;
    uint32_t active_connections = 0;
    uint32_t idle_connections = 0;
    uint32_t waiting_requests = 0;

    uint64_t total_acquires = 0;
    uint64_t total_releases = 0;
    uint64_t connections_created = 0;
    uint64_t connections_destroyed = 0;
    uint64_t acquire_timeouts = 0;
    uint64_t validation_failures = 0;

    double avg_connection_lifetime_sec = 0.0;
    double avg_acquire_wait_ms = 0.0;
};

/**
 * @brief Manages connections for a specific local user to a specific server
 */
class UserPool {
public:
    UserPool(ServerPool* parent,
             const std::string& local_user,
             const UserMapping& mapping);
    ~UserPool();

    // No copying
    UserPool(const UserPool&) = delete;
    UserPool& operator=(const UserPool&) = delete;

    /**
     * @brief Acquire a connection from the pool
     * @param timeout Maximum wait time
     * @return Pooled connection or error
     */
    Result<std::unique_ptr<IProtocolAdapter>> acquire(
        std::chrono::milliseconds timeout);

    /**
     * @brief Release a connection back to the pool
     * @param adapter Connection to release
     * @param failed Mark connection as failed (will be destroyed)
     */
    void release(std::unique_ptr<IProtocolAdapter> adapter, bool failed = false);

    /**
     * @brief Pre-create connections
     * @param count Number of connections to create
     */
    Result<void> warmup(uint32_t count);

    /**
     * @brief Evict idle connections beyond max_idle time
     * @param max_idle Maximum idle time
     */
    Result<void> evictIdle(std::chrono::milliseconds max_idle);

    /**
     * @brief Close all connections
     */
    void closeAll();

    /**
     * @brief Get pool statistics
     */
    UserPoolStats getStats() const;

    /**
     * @brief Get local user name
     */
    const std::string& getLocalUser() const { return local_user_; }

private:
    struct PooledConnection {
        std::unique_ptr<IProtocolAdapter> adapter;
        ConnectionState state = ConnectionState::DISCONNECTED;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point last_used;
        std::chrono::steady_clock::time_point last_validated;
        uint64_t use_count = 0;
        uint64_t query_count = 0;
        bool marked_for_eviction = false;

        // Default constructor for Result<T>
        PooledConnection() = default;
        // Move constructor
        PooledConnection(PooledConnection&&) = default;
        PooledConnection& operator=(PooledConnection&&) = default;
        // No copy
        PooledConnection(const PooledConnection&) = delete;
        PooledConnection& operator=(const PooledConnection&) = delete;
    };

    Result<PooledConnection> createConnection();
    Result<void> validateConnection(PooledConnection& conn);
    void destroyConnection(PooledConnection& conn);
    size_t activeCount() const;

    ServerPool* parent_;
    std::string local_user_;
    UserMapping mapping_;

    std::vector<PooledConnection> connections_;
    std::queue<size_t> idle_queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    uint32_t waiting_count_ = 0;
    mutable UserPoolStats stats_;
};

// =============================================================================
// ServerPool - Per-server connection pool
// =============================================================================

/**
 * @brief Manages connections to a single remote server
 */
class ServerPool {
public:
    explicit ServerPool(const ServerDefinition& server);
    ~ServerPool();

    // No copying
    ServerPool(const ServerPool&) = delete;
    ServerPool& operator=(const ServerPool&) = delete;

    /**
     * @brief Initialize the pool
     */
    Result<void> initialize();

    /**
     * @brief Shutdown the pool
     */
    Result<void> shutdown();

    /**
     * @brief Acquire a connection for a user
     * @param local_user Local username
     * @param mapping User credentials
     * @param timeout Maximum wait time
     * @return Connection or error
     */
    Result<std::unique_ptr<IProtocolAdapter>> acquire(
        const std::string& local_user,
        const UserMapping& mapping,
        std::chrono::milliseconds timeout);

    /**
     * @brief Release a connection back to the pool
     * @param local_user Local username
     * @param adapter Connection to release
     * @param failed Mark connection as failed
     */
    void release(const std::string& local_user,
                 std::unique_ptr<IProtocolAdapter> adapter,
                 bool failed = false);

    /**
     * @brief Set minimum connections
     */
    void setMinConnections(uint32_t min);

    /**
     * @brief Set maximum connections
     */
    void setMaxConnections(uint32_t max);

    /**
     * @brief Resize pool to target number of connections
     */
    Result<void> resize(uint32_t target);

    /**
     * @brief Evict idle connections
     */
    Result<void> evictIdle();

    /**
     * @brief Validate a specific connection
     */
    Result<void> validateConnection(IProtocolAdapter* conn);

    /**
     * @brief Validate all connections
     */
    Result<void> validateAll();

    /**
     * @brief Check if server is healthy
     */
    bool isHealthy() const { return healthy_.load(); }

    /**
     * @brief Mark server as unhealthy
     */
    void markUnhealthy(const std::string& reason);

    /**
     * @brief Mark server as healthy
     */
    void markHealthy();

    /**
     * @brief Get server definition
     */
    const ServerDefinition& getServerDefinition() const { return server_; }

    /**
     * @brief Update server definition
     */
    void updateDefinition(const ServerDefinition& server);

    /**
     * @brief Get pool statistics
     */
    ServerPoolStats getStats() const;

    /**
     * @brief Create a protocol adapter for this server type
     */
    std::unique_ptr<IProtocolAdapter> createAdapter();

private:
    UserPool* getOrCreateUserPool(const std::string& local_user,
                                   const UserMapping& mapping);

    ServerDefinition server_;
    std::unordered_map<std::string, std::unique_ptr<UserPool>> user_pools_;
    mutable std::shared_mutex user_pools_mutex_;

    std::atomic<bool> healthy_{true};
    std::string unhealthy_reason_;
    std::chrono::steady_clock::time_point last_healthy_;
    std::atomic<uint32_t> consecutive_failures_{0};

    std::chrono::steady_clock::time_point created_at_;
    std::atomic<uint64_t> total_queries_{0};
};

// =============================================================================
// PooledRemoteConnection - RAII wrapper for acquired connections
// =============================================================================

/**
 * @brief RAII wrapper for pooled remote connections
 *
 * Automatically releases the connection back to the pool when destroyed.
 */
class PooledRemoteConnection {
public:
    // Default constructor (required for Result<T>)
    PooledRemoteConnection() = default;

    // Move-only semantics
    PooledRemoteConnection(PooledRemoteConnection&& other) noexcept;
    PooledRemoteConnection& operator=(PooledRemoteConnection&& other) noexcept;
    PooledRemoteConnection(const PooledRemoteConnection&) = delete;
    PooledRemoteConnection& operator=(const PooledRemoteConnection&) = delete;

    /**
     * @brief Destructor releases connection back to pool
     */
    ~PooledRemoteConnection();

    /**
     * @brief Access underlying adapter
     */
    IProtocolAdapter* operator->() { return adapter_.get(); }
    IProtocolAdapter& operator*() { return *adapter_; }
    IProtocolAdapter* get() { return adapter_.get(); }
    const IProtocolAdapter* get() const { return adapter_.get(); }

    /**
     * @brief Check validity
     */
    bool valid() const { return adapter_ != nullptr; }
    explicit operator bool() const { return valid(); }

    /**
     * @brief Get server name
     */
    const std::string& getServerName() const { return server_name_; }

    /**
     * @brief Get local user name
     */
    const std::string& getLocalUser() const { return local_user_; }

    /**
     * @brief Release connection early (returns to pool)
     */
    void release();

    /**
     * @brief Mark connection as failed (will be destroyed instead of reused)
     */
    void markFailed() { failed_ = true; }

private:
    friend class RemoteConnectionPoolRegistry;

    PooledRemoteConnection(
        std::unique_ptr<IProtocolAdapter> adapter,
        RemoteConnectionPoolRegistry* registry,
        const std::string& server_name,
        const std::string& local_user);

    std::unique_ptr<IProtocolAdapter> adapter_;
    RemoteConnectionPoolRegistry* registry_ = nullptr;
    std::string server_name_;
    std::string local_user_;
    bool failed_ = false;
    bool released_ = false;
};

// =============================================================================
// HealthChecker - Background health monitoring
// =============================================================================

/**
 * @brief Background health checker for remote connections
 */
class HealthChecker {
public:
    explicit HealthChecker(RemoteConnectionPoolRegistry* registry);
    ~HealthChecker();

    /**
     * @brief Start health checker thread
     */
    void start();

    /**
     * @brief Stop health checker thread
     */
    void stop();

    /**
     * @brief Trigger immediate check for a server
     */
    void checkNow(const std::string& server_name);

    /**
     * @brief Set health check configuration
     */
    void setConfig(const HealthCheckConfig& config) { config_ = config; }

    /**
     * @brief Get health check configuration
     */
    const HealthCheckConfig& getConfig() const { return config_; }

private:
    void runLoop();
    void checkServer(ServerPool* pool);
    void checkConnection(IProtocolAdapter* conn);

    RemoteConnectionPoolRegistry* registry_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::condition_variable cv_;
    std::mutex mutex_;
    std::string immediate_check_server_;

    HealthCheckConfig config_;
};

// =============================================================================
// RemoteConnectionPoolRegistry - Singleton pool manager
// =============================================================================

/**
 * @brief Singleton registry for all remote connection pools
 *
 * Entry point for acquiring connections to remote databases.
 */
class RemoteConnectionPoolRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static RemoteConnectionPoolRegistry& instance();

    /**
     * @brief Initialize the registry (call once at startup)
     */
    void initialize();

    /**
     * @brief Shutdown the registry (call once at shutdown)
     */
    void shutdown();

    // =========================================================================
    // Server Pool Management
    // =========================================================================

    /**
     * @brief Register a new server
     * @param server Server definition
     */
    Result<void> registerServer(const ServerDefinition& server);

    /**
     * @brief Unregister a server
     * @param server_name Server name
     */
    Result<void> unregisterServer(const std::string& server_name);

    /**
     * @brief Update server configuration
     * @param server Updated server definition
     */
    Result<void> updateServer(const ServerDefinition& server);

    /**
     * @brief Get server definition by name
     * @param server_name Server name
     * @return Server definition or nullptr
     */
    const ServerDefinition* getServer(const std::string& server_name) const;

    /**
     * @brief List all registered servers
     */
    std::vector<std::string> listServers() const;

    // =========================================================================
    // Connection Acquisition
    // =========================================================================

    /**
     * @brief Acquire a connection to a remote server
     * @param server_name Server name
     * @param local_user Local username for credentials lookup
     * @param timeout Maximum wait time
     * @return Pooled connection or error
     */
    Result<PooledRemoteConnection> acquire(
        const std::string& server_name,
        const std::string& local_user,
        std::chrono::milliseconds timeout = std::chrono::seconds(5));

    /**
     * @brief Release a connection back to the pool
     * @param conn Connection to release
     *
     * Note: PooledRemoteConnection auto-releases on destruction
     */
    void release(PooledRemoteConnection&& conn);

    // =========================================================================
    // User Mapping Management
    // =========================================================================

    /**
     * @brief Add user mapping
     * @param mapping User mapping definition
     */
    Result<void> addUserMapping(const UserMapping& mapping);

    /**
     * @brief Remove user mapping
     * @param server_id Server ID
     * @param local_user Local username
     */
    Result<void> removeUserMapping(uint64_t server_id, const std::string& local_user);

    /**
     * @brief Get user mapping
     * @param server_name Server name
     * @param local_user Local username
     * @return User mapping or nullptr
     */
    const UserMapping* getUserMapping(const std::string& server_name,
                                       const std::string& local_user) const;

    // =========================================================================
    // Pool Operations
    // =========================================================================

    /**
     * @brief Warm up server pool
     * @param server_name Server name
     * @param count Number of connections to pre-create
     */
    Result<void> warmupServer(const std::string& server_name, uint32_t count);

    /**
     * @brief Evict idle connections from a server pool
     */
    Result<void> evictIdleConnections(const std::string& server_name);

    /**
     * @brief Close all connections to a server
     */
    Result<void> closeAllConnections(const std::string& server_name);

    // =========================================================================
    // Health and Monitoring
    // =========================================================================

    /**
     * @brief Check if server is healthy
     */
    bool isServerHealthy(const std::string& server_name) const;

    /**
     * @brief Get registry statistics
     */
    RegistryStats getStats() const;

    /**
     * @brief Get server pool statistics
     */
    ServerPoolStats getServerStats(const std::string& server_name) const;

    /**
     * @brief Start health checker thread
     */
    void startHealthChecker();

    /**
     * @brief Stop health checker thread
     */
    void stopHealthChecker();

    /**
     * @brief Get server pool by name (internal use)
     */
    ServerPool* getServerPool(const std::string& server_name);

private:
    RemoteConnectionPoolRegistry();
    ~RemoteConnectionPoolRegistry();

    // No copying
    RemoteConnectionPoolRegistry(const RemoteConnectionPoolRegistry&) = delete;
    RemoteConnectionPoolRegistry& operator=(const RemoteConnectionPoolRegistry&) = delete;

    const UserMapping* findUserMapping(uint64_t server_id,
                                        const std::string& local_user) const;

    std::unordered_map<std::string, std::unique_ptr<ServerPool>> server_pools_;
    mutable std::shared_mutex server_pools_mutex_;

    // User mappings: key = server_id
    std::unordered_map<uint64_t, std::vector<UserMapping>> user_mappings_;
    mutable std::shared_mutex user_mappings_mutex_;

    // Health checker
    std::unique_ptr<HealthChecker> health_checker_;
    std::atomic<bool> initialized_{false};

    // Configuration
    std::chrono::milliseconds health_check_interval_{30000};
};

// =============================================================================
// Connection Guard - RAII helper for scoped connection usage
// =============================================================================

/**
 * @brief RAII guard for automatic connection release
 */
class ConnectionGuard {
public:
    explicit ConnectionGuard(PooledRemoteConnection&& conn)
        : conn_(std::move(conn)) {}

    ~ConnectionGuard() = default;  // PooledRemoteConnection releases on destruction

    IProtocolAdapter* operator->() { return conn_.get(); }
    IProtocolAdapter& operator*() { return *conn_; }
    IProtocolAdapter* get() { return conn_.get(); }

    bool valid() const { return conn_.valid(); }
    void markFailed() { conn_.markFailed(); }

private:
    PooledRemoteConnection conn_;
};

}  // namespace fdw
}  // namespace scratchbird

#endif  // SCRATCHBIRD_FDW_REMOTE_CONNECTION_POOL_H

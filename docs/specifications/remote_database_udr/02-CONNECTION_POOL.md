# Remote Database UDR - Connection Pool

> Reference-only: Canonical UDR and live-migration behavior now lives in
> `ScratchBird/docs/specifications/Alpha Phase 2/11-Remote-Database-UDR-Specification.md`
> and `ScratchBird/docs/specifications/Alpha Phase 2/11h-Live-Migration-Emulated-Listener.md`.

## 1. Overview

The Remote Connection Pool manages connections to external databases, providing:
- Connection pooling per server/user combination
- Health monitoring and automatic recovery
- Connection lifecycle management
- Statistics and metrics collection

**Scope Note:** MSSQL/TDS adapter support is post-gold; MSSQL references are forward-looking.

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  RemoteConnectionPoolRegistry (Singleton)                    │
│  - Manages all server pools                                  │
│  - Server discovery and lifecycle                            │
│  - Global statistics                                         │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────────────┐  ┌──────────────────────┐        │
│  │  ServerPool          │  │  ServerPool          │        │
│  │  (legacy_pg)         │  │  (prod_mysql)        │        │
│  │                      │  │                      │        │
│  │  ┌────────────────┐  │  │  ┌────────────────┐  │        │
│  │  │ UserPool       │  │  │  │ UserPool       │  │        │
│  │  │ (admin)        │  │  │  │ (app_user)     │  │        │
│  │  │ [conn][conn]   │  │  │  │ [conn][conn]   │  │        │
│  │  └────────────────┘  │  │  └────────────────┘  │        │
│  │                      │  │                      │        │
│  │  ┌────────────────┐  │  │  ┌────────────────┐  │        │
│  │  │ UserPool       │  │  │  │ UserPool       │  │        │
│  │  │ (readonly)     │  │  │  │ (migration)    │  │        │
│  │  │ [conn]         │  │  │  │ [conn][conn]   │  │        │
│  │  └────────────────┘  │  │  └────────────────┘  │        │
│  └──────────────────────┘  └──────────────────────┘        │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. RemoteConnectionPoolRegistry

The global registry manages all server pools.

```cpp
class RemoteConnectionPoolRegistry {
public:
    // Singleton access
    static RemoteConnectionPoolRegistry& instance();

    // Server pool management
    Result<void> registerServer(const ServerDefinition& server);
    Result<void> unregisterServer(const std::string& server_name);
    Result<void> updateServer(const ServerDefinition& server);

    // Connection acquisition (main entry point)
    Result<PooledRemoteConnection> acquire(
        const std::string& server_name,
        const std::string& local_user,
        std::chrono::milliseconds timeout = std::chrono::seconds(5));

    // Connection release
    void release(PooledRemoteConnection&& conn);

    // Pool operations
    Result<void> warmupServer(const std::string& server_name, uint32_t count);
    Result<void> evictIdleConnections(const std::string& server_name);
    Result<void> closeAllConnections(const std::string& server_name);

    // Health and monitoring
    bool isServerHealthy(const std::string& server_name) const;
    RegistryStats getStats() const;
    ServerPoolStats getServerStats(const std::string& server_name) const;

    // Background tasks
    void startHealthChecker();
    void stopHealthChecker();

private:
    RemoteConnectionPoolRegistry();
    ~RemoteConnectionPoolRegistry();

    // Server pools keyed by server name
    std::unordered_map<std::string, std::unique_ptr<ServerPool>> server_pools_;
    mutable std::shared_mutex mutex_;

    // Health checker thread
    std::thread health_checker_thread_;
    std::atomic<bool> health_checker_running_{false};
    std::condition_variable_any health_checker_cv_;

    // Configuration
    std::chrono::milliseconds health_check_interval_{30000};
};

struct RegistryStats {
    uint32_t total_servers;
    uint32_t healthy_servers;
    uint32_t unhealthy_servers;
    uint64_t total_connections;
    uint64_t active_connections;
    uint64_t total_acquires;
    uint64_t total_releases;
    uint64_t total_errors;
};
```

---

## 4. ServerPool

Manages connections to a single remote server.

```cpp
class ServerPool {
public:
    explicit ServerPool(const ServerDefinition& server);
    ~ServerPool();

    // Initialize the pool (create min connections)
    Result<void> initialize();
    Result<void> shutdown();

    // Connection operations
    Result<PooledRemoteConnection> acquire(
        const std::string& local_user,
        const UserMapping& mapping,
        std::chrono::milliseconds timeout);
    void release(PooledRemoteConnection&& conn);

    // Pool sizing
    void setMinConnections(uint32_t min);
    void setMaxConnections(uint32_t max);
    Result<void> resize(uint32_t target);
    Result<void> evictIdle();

    // Health
    Result<void> validateConnection(IProtocolAdapter* conn);
    Result<void> validateAll();
    bool isHealthy() const;
    void markUnhealthy(const std::string& reason);
    void markHealthy();

    // Configuration
    const ServerDefinition& getServerDefinition() const { return server_; }
    void updateDefinition(const ServerDefinition& server);

    // Statistics
    ServerPoolStats getStats() const;

private:
    ServerDefinition server_;

    // User pools (keyed by local username)
    std::unordered_map<std::string, std::unique_ptr<UserPool>> user_pools_;
    mutable std::shared_mutex user_pools_mutex_;

    // Health state
    std::atomic<bool> healthy_{true};
    std::string unhealthy_reason_;
    std::chrono::steady_clock::time_point last_healthy_;
    uint32_t consecutive_failures_{0};

    // Statistics
    mutable std::mutex stats_mutex_;
    ServerPoolStats stats_;

    // Create adapter based on database type
    std::unique_ptr<IProtocolAdapter> createAdapter();
    UserPool* getOrCreateUserPool(const std::string& local_user,
                                   const UserMapping& mapping);
};

struct ServerPoolStats {
    std::string server_name;
    RemoteDatabaseType db_type;
    bool healthy;
    std::string unhealthy_reason;

    uint32_t total_user_pools;
    uint32_t total_connections;
    uint32_t active_connections;
    uint32_t idle_connections;
    uint32_t pending_requests;

    uint64_t total_acquires;
    uint64_t total_releases;
    uint64_t acquire_timeouts;
    uint64_t connection_errors;
    uint64_t queries_executed;

    double avg_acquire_time_ms;
    double avg_query_time_ms;

    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_activity;
};
```

---

## 5. UserPool

Manages connections for a specific local user to a specific server.

```cpp
class UserPool {
public:
    UserPool(ServerPool* parent,
             const std::string& local_user,
             const UserMapping& mapping);
    ~UserPool();

    // Connection operations
    Result<PooledRemoteConnection> acquire(std::chrono::milliseconds timeout);
    void release(PooledRemoteConnection&& conn);

    // Pool management
    Result<void> warmup(uint32_t count);
    Result<void> evictIdle(std::chrono::milliseconds max_idle);
    void closeAll();

    // Statistics
    UserPoolStats getStats() const;

private:
    ServerPool* parent_;
    std::string local_user_;
    UserMapping mapping_;

    // Connection storage
    struct PooledConnection {
        std::unique_ptr<IProtocolAdapter> adapter;
        ConnectionState state;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point last_used;
        std::chrono::steady_clock::time_point last_validated;
        uint64_t use_count;
        uint64_t query_count;
        bool marked_for_eviction;
    };

    std::vector<PooledConnection> connections_;
    std::queue<size_t> idle_queue_;  // Indices of idle connections
    mutable std::mutex mutex_;
    std::condition_variable cv_;

    // Waiting requests
    uint32_t waiting_count_{0};

    // Statistics
    UserPoolStats stats_;

    // Create new connection
    Result<PooledConnection> createConnection();
    Result<void> validateConnection(PooledConnection& conn);
    void destroyConnection(PooledConnection& conn);
};

struct UserPoolStats {
    std::string local_user;
    uint32_t total_connections;
    uint32_t active_connections;
    uint32_t idle_connections;
    uint32_t waiting_requests;

    uint64_t total_acquires;
    uint64_t total_releases;
    uint64_t connections_created;
    uint64_t connections_destroyed;
    uint64_t acquire_timeouts;
    uint64_t validation_failures;

    double avg_connection_lifetime_sec;
    double avg_acquire_wait_ms;
};
```

---

## 6. PooledRemoteConnection

RAII wrapper for acquired connections.

```cpp
class PooledRemoteConnection {
public:
    // Move-only semantics
    PooledRemoteConnection(PooledRemoteConnection&& other) noexcept;
    PooledRemoteConnection& operator=(PooledRemoteConnection&& other) noexcept;
    PooledRemoteConnection(const PooledRemoteConnection&) = delete;
    PooledRemoteConnection& operator=(const PooledRemoteConnection&) = delete;

    // Destructor releases back to pool
    ~PooledRemoteConnection();

    // Access underlying adapter
    IProtocolAdapter* operator->() { return adapter_.get(); }
    IProtocolAdapter& operator*() { return *adapter_; }
    IProtocolAdapter* get() { return adapter_.get(); }

    // Check validity
    bool valid() const { return adapter_ != nullptr; }
    explicit operator bool() const { return valid(); }

    // Connection info
    const std::string& getServerName() const { return server_name_; }
    const std::string& getLocalUser() const { return local_user_; }

    // Release early (returns to pool)
    void release();

    // Mark as failed (will be destroyed instead of reused)
    void markFailed();

private:
    friend class UserPool;
    friend class RemoteConnectionPoolRegistry;

    PooledRemoteConnection(
        std::unique_ptr<IProtocolAdapter> adapter,
        RemoteConnectionPoolRegistry* registry,
        const std::string& server_name,
        const std::string& local_user);

    std::unique_ptr<IProtocolAdapter> adapter_;
    RemoteConnectionPoolRegistry* registry_;
    std::string server_name_;
    std::string local_user_;
    bool failed_{false};
    bool released_{false};
};
```

---

## 7. Health Checking

### 7.1 Health Check Types

```cpp
enum class HealthCheckType {
    PING,           // Simple connection test
    QUERY,          // Execute test query
    FULL_VALIDATE,  // Reset and validate fully
};

struct HealthCheckConfig {
    HealthCheckType type = HealthCheckType::PING;
    std::string test_query = "SELECT 1";  // For QUERY type
    std::chrono::milliseconds timeout{5000};
    std::chrono::milliseconds interval{30000};
    uint32_t failure_threshold = 3;       // Mark unhealthy after N failures
    uint32_t recovery_threshold = 2;      // Mark healthy after N successes
};
```

### 7.2 Health Checker Implementation

```cpp
class HealthChecker {
public:
    explicit HealthChecker(RemoteConnectionPoolRegistry* registry);

    void start();
    void stop();
    void checkNow(const std::string& server_name);

private:
    void runLoop();
    void checkServer(ServerPool* pool);
    void checkConnection(IProtocolAdapter* conn);

    RemoteConnectionPoolRegistry* registry_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::condition_variable cv_;
    std::mutex mutex_;

    HealthCheckConfig config_;
};
```

### 7.3 Health Check Queries by Database Type

```cpp
const std::unordered_map<RemoteDatabaseType, std::string> health_check_queries = {
    {RemoteDatabaseType::POSTGRESQL, "SELECT 1"},
    {RemoteDatabaseType::MYSQL, "SELECT 1"},
    {RemoteDatabaseType::MSSQL, "SELECT 1"},
    {RemoteDatabaseType::FIREBIRD, "SELECT 1 FROM RDB$DATABASE"},
    {RemoteDatabaseType::SCRATCHBIRD, "SELECT 1"},
};
```

---

## 8. Connection Lifecycle

### 8.1 Connection Creation

```cpp
Result<PooledConnection> UserPool::createConnection() {
    // 1. Create protocol adapter based on database type
    auto adapter = parent_->createAdapter();
    if (!adapter) {
        return Error(remote_sqlstate::CONNECTION_FAILED,
                     "Failed to create protocol adapter");
    }

    // 2. Establish connection
    auto connect_result = adapter->connect(
        parent_->getServerDefinition(),
        mapping_);
    if (!connect_result) {
        return Error(remote_sqlstate::CONNECTION_FAILED,
                     connect_result.error().message());
    }

    // 3. Initialize connection state
    PooledConnection conn;
    conn.adapter = std::move(adapter);
    conn.state = ConnectionState::CONNECTED;
    conn.created_at = std::chrono::steady_clock::now();
    conn.last_used = conn.created_at;
    conn.last_validated = conn.created_at;
    conn.use_count = 0;
    conn.query_count = 0;
    conn.marked_for_eviction = false;

    // 4. Update statistics
    stats_.connections_created++;

    return conn;
}
```

### 8.2 Connection Acquisition

```cpp
Result<PooledRemoteConnection> UserPool::acquire(
    std::chrono::milliseconds timeout)
{
    std::unique_lock lock(mutex_);
    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (true) {
        // 1. Try to get idle connection
        if (!idle_queue_.empty()) {
            size_t idx = idle_queue_.front();
            idle_queue_.pop();

            auto& conn = connections_[idx];
            conn.state = ConnectionState::EXECUTING;
            conn.last_used = std::chrono::steady_clock::now();
            conn.use_count++;

            stats_.total_acquires++;

            return PooledRemoteConnection(
                std::move(conn.adapter),
                RemoteConnectionPoolRegistry::instance(),
                parent_->getServerDefinition().server_name,
                local_user_);
        }

        // 2. Can we create new connection?
        if (connections_.size() < parent_->getServerDefinition().max_connections) {
            lock.unlock();
            auto result = createConnection();
            if (!result) {
                return result.error();
            }

            lock.lock();
            connections_.push_back(std::move(*result));
            auto& conn = connections_.back();
            conn.state = ConnectionState::EXECUTING;
            conn.use_count = 1;

            stats_.total_acquires++;

            return PooledRemoteConnection(
                std::move(conn.adapter),
                &RemoteConnectionPoolRegistry::instance(),
                parent_->getServerDefinition().server_name,
                local_user_);
        }

        // 3. Wait for available connection
        waiting_count_++;
        if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            waiting_count_--;
            stats_.acquire_timeouts++;
            return Error(remote_sqlstate::POOL_EXHAUSTED,
                         "Timeout waiting for connection");
        }
        waiting_count_--;
    }
}
```

### 8.3 Connection Release

```cpp
void UserPool::release(PooledRemoteConnection&& conn) {
    std::lock_guard lock(mutex_);

    // Find the connection slot
    for (size_t i = 0; i < connections_.size(); ++i) {
        if (connections_[i].adapter.get() == conn.get()) {
            auto& pooled = connections_[i];

            // Check if connection is still valid
            if (conn.failed_ || !pooled.adapter->isConnected()) {
                destroyConnection(pooled);
                connections_.erase(connections_.begin() + i);
            } else {
                // Reset connection state
                if (pooled.adapter->getState() == ConnectionState::IN_TRANSACTION) {
                    pooled.adapter->rollback();
                }
                pooled.adapter->reset();

                pooled.state = ConnectionState::CONNECTED;
                pooled.last_used = std::chrono::steady_clock::now();

                // Return to idle queue
                idle_queue_.push(i);
            }

            stats_.total_releases++;

            // Notify waiting acquirers
            cv_.notify_one();
            return;
        }
    }
}
```

---

## 9. Idle Connection Eviction

```cpp
Result<void> UserPool::evictIdle(std::chrono::milliseconds max_idle) {
    std::lock_guard lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto min_conns = parent_->getServerDefinition().min_connections;

    // Build new idle queue, evicting old connections
    std::queue<size_t> new_idle_queue;

    while (!idle_queue_.empty()) {
        size_t idx = idle_queue_.front();
        idle_queue_.pop();

        auto& conn = connections_[idx];
        auto idle_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - conn.last_used);

        // Keep if under max idle time or at minimum connections
        if (idle_time < max_idle ||
            new_idle_queue.size() + activeCount() < min_conns) {
            new_idle_queue.push(idx);
        } else {
            conn.marked_for_eviction = true;
        }
    }

    idle_queue_ = std::move(new_idle_queue);

    // Destroy marked connections
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
            [this](PooledConnection& c) {
                if (c.marked_for_eviction) {
                    destroyConnection(c);
                    return true;
                }
                return false;
            }),
        connections_.end());

    return {};
}
```

---

## 10. Configuration

### 10.1 Pool Configuration Options

```cpp
struct PoolConfiguration {
    // Pool sizing
    uint32_t min_connections = 1;
    uint32_t max_connections = 10;

    // Timeouts
    std::chrono::milliseconds connect_timeout{5000};
    std::chrono::milliseconds query_timeout{30000};
    std::chrono::milliseconds acquire_timeout{5000};
    std::chrono::milliseconds idle_timeout{300000};   // 5 minutes
    std::chrono::milliseconds max_lifetime{3600000};  // 1 hour

    // Health checking
    bool enable_health_check = true;
    std::chrono::milliseconds health_check_interval{30000};
    HealthCheckType health_check_type = HealthCheckType::PING;
    std::string health_check_query = "SELECT 1";

    // Connection behavior
    bool reset_on_release = true;
    bool validate_on_acquire = false;
    bool validate_on_release = false;
    uint32_t validation_interval_seconds = 30;

    // Warmup
    bool warmup_on_create = true;
    uint32_t warmup_connections = 1;

    // Statistics
    bool collect_statistics = true;
    bool collect_query_stats = true;
};
```

### 10.2 SQL Configuration

Pool settings can be configured via CREATE SERVER OPTIONS:

```sql
CREATE SERVER legacy_pg
    FOREIGN DATA WRAPPER postgresql_fdw
    OPTIONS (
        host 'db.example.com',
        port '5432',
        dbname 'production',
        -- Pool configuration
        min_connections '2',
        max_connections '20',
        connect_timeout '5000',
        query_timeout '30000',
        idle_timeout '300000',
        -- Health check
        health_check_interval '30000',
        health_check_query 'SELECT 1',
        -- Behavior
        reset_on_release 'true',
        validate_on_acquire 'false'
    );
```

---

## 11. Statistics and Metrics

### 11.1 Prometheus Metrics

```prometheus
# Pool-level metrics
scratchbird_remote_pool_connections_total{server="legacy_pg",state="active"} 5
scratchbird_remote_pool_connections_total{server="legacy_pg",state="idle"} 3
scratchbird_remote_pool_connections_total{server="legacy_pg",state="failed"} 0

scratchbird_remote_pool_acquires_total{server="legacy_pg"} 15234
scratchbird_remote_pool_releases_total{server="legacy_pg"} 15230
scratchbird_remote_pool_timeouts_total{server="legacy_pg"} 2
scratchbird_remote_pool_errors_total{server="legacy_pg"} 5

scratchbird_remote_pool_acquire_duration_seconds{server="legacy_pg",quantile="0.5"} 0.001
scratchbird_remote_pool_acquire_duration_seconds{server="legacy_pg",quantile="0.99"} 0.05

# Connection-level metrics
scratchbird_remote_connection_queries_total{server="legacy_pg"} 45678
scratchbird_remote_connection_query_duration_seconds{server="legacy_pg",quantile="0.5"} 0.01
scratchbird_remote_connection_bytes_sent_total{server="legacy_pg"} 12345678
scratchbird_remote_connection_bytes_received_total{server="legacy_pg"} 98765432

# Health metrics
scratchbird_remote_pool_healthy{server="legacy_pg"} 1
scratchbird_remote_pool_health_checks_total{server="legacy_pg",result="success"} 100
scratchbird_remote_pool_health_checks_total{server="legacy_pg",result="failure"} 0
```

### 11.2 SQL Statistics Views

```sql
-- View pool statistics
SELECT * FROM sys.remote_pool_stats;

-- Result:
-- server_name | db_type    | healthy | total_conns | active | idle | waiting | acquires | timeouts
-- legacy_pg   | postgresql | true    | 8           | 5      | 3    | 0       | 15234    | 2
-- prod_mysql  | mysql      | true    | 4           | 2      | 2    | 0       | 8901     | 0

-- View per-user statistics
SELECT * FROM sys.remote_user_pool_stats WHERE server_name = 'legacy_pg';

-- View connection details
SELECT * FROM sys.remote_connections WHERE server_name = 'legacy_pg';
```

---

## 12. Thread Safety

### 12.1 Lock Hierarchy

To prevent deadlocks, locks must be acquired in this order:

1. `RemoteConnectionPoolRegistry::mutex_` (registry-level)
2. `ServerPool::user_pools_mutex_` (server-level)
3. `UserPool::mutex_` (user-level)

### 12.2 Lock-Free Operations

Where possible, use atomic operations:

```cpp
class ServerPool {
    std::atomic<bool> healthy_{true};
    std::atomic<uint32_t> consecutive_failures_{0};
    std::atomic<uint64_t> total_queries_{0};
};
```

---

## 13. Error Handling

### 13.1 Connection Errors

```cpp
Result<void> handleConnectionError(
    PooledConnection& conn,
    const std::exception& e)
{
    // Log error
    LOG_ERROR("Remote connection error: {}", e.what());

    // Update statistics
    stats_.connection_errors++;

    // Determine if recoverable
    if (isRecoverableError(e)) {
        // Try to reset connection
        auto reset_result = conn.adapter->reset();
        if (reset_result) {
            return {};  // Recovered
        }
    }

    // Mark connection as failed
    conn.state = ConnectionState::FAILED;
    parent_->markConnectionFailed();

    return Error(remote_sqlstate::CONNECTION_FAILED, e.what());
}

bool isRecoverableError(const std::exception& e) {
    // Timeout and temporary network errors are recoverable
    // Protocol errors and auth failures are not
    // ... implementation
}
```

### 13.2 Pool Exhaustion

```cpp
// When pool is exhausted, options:
// 1. Wait with timeout (default)
// 2. Fail fast
// 3. Create temporary overflow connection

enum class ExhaustionPolicy {
    WAIT,           // Wait for available connection
    FAIL_FAST,      // Return error immediately
    OVERFLOW,       // Create temporary connection beyond max
};
```

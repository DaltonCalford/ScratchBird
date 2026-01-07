# ScratchBird Connection Pooling Specification

**Version:** 1.0
**Date:** December 10, 2025
**Status:** Design Complete - Ready for Implementation
**Author:** dcalford with Claude Code

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [Pool Lifecycle](#3-pool-lifecycle)
4. [Configuration](#4-configuration)
5. [Connection Management](#5-connection-management)
6. [Statement Caching](#6-statement-caching)
7. [Result Caching](#7-result-caching)
8. [Health Checking](#8-health-checking)
9. [Load Balancing](#9-load-balancing)
10. [Statistics and Monitoring](#10-statistics-and-monitoring)
11. [SQL Interface](#11-sql-interface)
12. [Implementation Details](#12-implementation-details)
13. [Security Considerations](#13-security-considerations)
14. [Troubleshooting](#14-troubleshooting)

---

## 1. Overview

ScratchBird implements a built-in connection pooling system that manages database connections efficiently. Unlike external poolers (pgBouncer, ProxySQL), the pool is integrated directly into the server, providing:

- **Zero network hop** between pooler and database
- **Full protocol support** for all wire protocols
- **Statement-level caching** with prepared statement sharing
- **Result-level caching** for repeated queries
- **MGA-aware** connection validation

### Design Goals

| Goal | Description |
|------|-------------|
| Performance | Minimize connection overhead, maximize throughput |
| Efficiency | Reuse connections, reduce resource consumption |
| Reliability | Health checking, automatic recovery |
| Observability | Comprehensive statistics and monitoring |
| Flexibility | Per-database, per-user, per-application pools |

### Pool Types

ScratchBird supports three pooling modes:

| Mode | Description | Use Case |
|------|-------------|----------|
| **Session** | Connection bound to client session | Long-running applications |
| **Transaction** | Connection returned after each transaction | Web applications, microservices |
| **Statement** | Connection returned after each statement | High-concurrency read workloads |

---

## 2. Architecture

### 2.1 Pool Hierarchy

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     Connection Pool Architecture                         │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│  Pool Manager (Singleton)                                                │
│  - Global configuration                                                  │
│  - Pool registry                                                         │
│  - Statistics aggregation                                                │
│  - Background maintenance                                                │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
            ┌───────────────────────┼───────────────────────┐
            │                       │                       │
            ▼                       ▼                       ▼
┌─────────────────────┐ ┌─────────────────────┐ ┌─────────────────────┐
│  Database Pool      │ │  Database Pool      │ │  Database Pool      │
│  (main)             │ │  (production)       │ │  (analytics)        │
│                     │ │                     │ │                     │
│  ┌───────────────┐  │ │  ┌───────────────┐  │ │  ┌───────────────┐  │
│  │ User Pool     │  │ │  │ User Pool     │  │ │  │ User Pool     │  │
│  │ (alice)       │  │ │  │ (app_user)    │  │ │  │ (analyst)     │  │
│  │ ┌───────────┐ │  │ │  │ ┌───────────┐ │  │ │  │ ┌───────────┐ │  │
│  │ │Connection │ │  │ │  │ │Connection │ │  │ │  │ │Connection │ │  │
│  │ │Connection │ │  │ │  │ │Connection │ │  │ │  │ │Connection │ │  │
│  │ │Connection │ │  │ │  │ │Connection │ │  │ │  │ │Connection │ │  │
│  │ └───────────┘ │  │ │  │ └───────────┘ │  │ │  │ └───────────┘ │  │
│  └───────────────┘  │ │  └───────────────┘  │ │  └───────────────┘  │
│                     │ │                     │ │                     │
│  Statement Cache    │ │  Statement Cache    │ │  Statement Cache    │
│  Result Cache       │ │  Result Cache       │ │  Result Cache       │
└─────────────────────┘ └─────────────────────┘ └─────────────────────┘
```

### 2.2 Connection States

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     Connection State Machine                             │
└─────────────────────────────────────────────────────────────────────────┘

                    ┌──────────────┐
                    │   CREATED    │
                    │  (new conn)  │
                    └──────┬───────┘
                           │ authenticate()
                           ▼
                    ┌──────────────┐
        ┌──────────│    IDLE      │◄─────────────────────┐
        │          │  (in pool)   │                      │
        │          └──────┬───────┘                      │
        │                 │ acquire()                    │
        │                 ▼                              │
        │          ┌──────────────┐                      │
        │          │   ACQUIRED   │                      │
        │          │  (in use)    │──────────────────────┤
        │          └──────┬───────┘     release()       │
        │                 │                              │
        │                 │ begin_txn()                  │
        │                 ▼                              │
        │          ┌──────────────┐                      │
        │          │ IN_TRANSACTION│                     │
        │          │              │──────────────────────┤
        │          └──────┬───────┘  commit()/rollback() │
        │                 │                              │
        │                 │ (transaction pool mode)      │
        │                 └──────────────────────────────┘
        │
        │ validate() failed
        │ or max_lifetime exceeded
        │ or idle_timeout exceeded
        ▼
  ┌──────────────┐
  │   CLOSING    │
  │              │
  └──────┬───────┘
         │ close()
         ▼
  ┌──────────────┐
  │   CLOSED     │
  │  (removed)   │
  └──────────────┘
```

### 2.3 Component Overview

| Component | Responsibility |
|-----------|----------------|
| **PoolManager** | Global pool coordination, configuration, statistics |
| **DatabasePool** | Per-database connection management |
| **UserPool** | Per-user connection isolation (optional) |
| **PooledConnection** | Individual connection wrapper with metadata |
| **StatementCache** | Prepared statement sharing across connections |
| **ResultCache** | Query result caching |
| **HealthChecker** | Background connection validation |
| **Evictor** | Idle/expired connection cleanup |

---

## 3. Pool Lifecycle

### 3.1 Initialization

```c
// Pool initialization on server startup
Status PoolManager::initialize(const PoolConfig& global_config) {
    // 1. Load global configuration
    global_config_ = global_config;

    // 2. Create database pools based on config
    for (const auto& db_config : global_config.databases) {
        auto pool = std::make_unique<DatabasePool>(db_config);
        pools_[db_config.name] = std::move(pool);
    }

    // 3. Start background threads
    health_checker_ = std::thread(&PoolManager::healthCheckLoop, this);
    evictor_ = std::thread(&PoolManager::evictionLoop, this);
    stats_collector_ = std::thread(&PoolManager::statsLoop, this);

    // 4. Pre-warm pools if configured
    if (global_config.prewarm) {
        for (auto& [name, pool] : pools_) {
            pool->prewarm(global_config.prewarm_count);
        }
    }

    return Status::OK;
}
```

### 3.2 Connection Acquisition

```c
// Acquire connection from pool
StatusOr<PooledConnection*> DatabasePool::acquire(
    const std::string& user,
    std::chrono::milliseconds timeout
) {
    auto deadline = std::chrono::steady_clock::now() + timeout;

    std::unique_lock<std::mutex> lock(mutex_);

    while (true) {
        // 1. Try to get existing idle connection
        if (auto conn = tryGetIdleConnection(user)) {
            stats_.acquires++;
            conn->state = ConnectionState::ACQUIRED;
            conn->last_used = std::chrono::steady_clock::now();
            return conn;
        }

        // 2. Try to create new connection if under limit
        if (active_connections_ < config_.max_connections) {
            lock.unlock();
            auto conn = createConnection(user);
            if (conn.ok()) {
                stats_.creates++;
                return conn.value();
            }
            lock.lock();
        }

        // 3. Wait for available connection
        if (std::chrono::steady_clock::now() >= deadline) {
            stats_.timeouts++;
            return Status::POOL_TIMEOUT;
        }

        stats_.waits++;
        available_.wait_until(lock, deadline);
    }
}
```

### 3.3 Connection Release

```c
// Release connection back to pool
void DatabasePool::release(PooledConnection* conn) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Check if connection is still valid
    if (conn->needs_reset) {
        resetConnection(conn);
    }

    // 2. Check if connection should be closed
    if (shouldClose(conn)) {
        closeConnection(conn);
        stats_.closes++;
        return;
    }

    // 3. Return to idle pool
    conn->state = ConnectionState::IDLE;
    conn->idle_since = std::chrono::steady_clock::now();
    idle_connections_.push_back(conn);
    stats_.releases++;

    // 4. Notify waiters
    available_.notify_one();
}

bool DatabasePool::shouldClose(PooledConnection* conn) {
    auto now = std::chrono::steady_clock::now();

    // Max lifetime exceeded
    if (now - conn->created_at > config_.max_lifetime) {
        return true;
    }

    // Too many idle connections
    if (idle_connections_.size() >= config_.max_idle) {
        return true;
    }

    // Connection marked as broken
    if (conn->is_broken) {
        return true;
    }

    return false;
}
```

### 3.4 Connection Reset

When a connection is returned to the pool, it may need to be reset:

```c
void DatabasePool::resetConnection(PooledConnection* conn) {
    // 1. Rollback any uncommitted transaction
    if (conn->in_transaction) {
        conn->execute("ROLLBACK");
        conn->in_transaction = false;
    }

    // 2. Reset session variables to defaults
    conn->execute("RESET ALL");

    // 3. Clear any temporary tables
    conn->execute("DISCARD TEMP");

    // 4. Reset search path
    conn->execute("SET search_path TO DEFAULT");

    // 5. Clear prepared statements (if not caching)
    if (!config_.statement_cache_enabled) {
        conn->execute("DEALLOCATE ALL");
    }

    // 6. Clear any advisory locks
    conn->execute("SELECT pg_advisory_unlock_all()");

    conn->needs_reset = false;
}
```

### 3.5 Shutdown

```c
Status PoolManager::shutdown(std::chrono::seconds timeout) {
    // 1. Stop accepting new connections
    accepting_ = false;

    // 2. Signal shutdown to all pools
    for (auto& [name, pool] : pools_) {
        pool->initiateShutdown();
    }

    // 3. Wait for active connections to complete
    auto deadline = std::chrono::steady_clock::now() + timeout;
    for (auto& [name, pool] : pools_) {
        if (!pool->waitForIdle(deadline - std::chrono::steady_clock::now())) {
            // Force close remaining connections
            pool->forceCloseAll();
        }
    }

    // 4. Stop background threads
    shutdown_flag_ = true;
    health_checker_.join();
    evictor_.join();
    stats_collector_.join();

    // 5. Clear caches
    for (auto& [name, pool] : pools_) {
        pool->clearStatementCache();
        pool->clearResultCache();
    }

    return Status::OK;
}
```

---

## 4. Configuration

### 4.1 Global Configuration

```ini
# /etc/scratchbird/sb_server.conf

[pool]
#------------------------------------------------------------------------------
# POOL MODE
#------------------------------------------------------------------------------

# Pooling mode: session | transaction | statement
# - session: Connection bound to client for entire session
# - transaction: Connection returned after each COMMIT/ROLLBACK
# - statement: Connection returned after each statement (highest reuse)
mode = transaction

# Enable connection pooling (false = direct connections only)
enabled = true

#------------------------------------------------------------------------------
# CONNECTION LIMITS
#------------------------------------------------------------------------------

# Minimum idle connections to maintain per pool
# Connections below this are pre-created on startup
min_idle = 5

# Maximum idle connections per pool
# Excess idle connections are closed
max_idle = 20

# Maximum total connections per pool
# Includes both active and idle
max_connections = 100

# Maximum connections per user (0 = unlimited)
max_connections_per_user = 0

# Maximum total connections across all pools
max_total_connections = 500

#------------------------------------------------------------------------------
# TIMEOUTS
#------------------------------------------------------------------------------

# Maximum time to wait for connection from pool (milliseconds)
acquire_timeout = 30000

# Close idle connections after this duration (seconds, 0 = never)
idle_timeout = 300

# Maximum connection lifetime (seconds, 0 = unlimited)
# Connections older than this are closed when returned to pool
max_lifetime = 3600

# Validation timeout (milliseconds)
validation_timeout = 5000

# Connection timeout for new connections (milliseconds)
connect_timeout = 10000

#------------------------------------------------------------------------------
# VALIDATION
#------------------------------------------------------------------------------

# Validate connections before handing to client
validate_on_acquire = true

# Validate connections when returned to pool
validate_on_release = false

# Background validation interval (seconds, 0 = disabled)
validation_interval = 60

# Validation query (empty = use isValid() method)
validation_query = SELECT 1

# Number of consecutive failures before removing connection
max_validation_failures = 3

#------------------------------------------------------------------------------
# STATEMENT CACHE
#------------------------------------------------------------------------------

# Enable prepared statement caching
statement_cache_enabled = true

# Maximum cached statements per connection
statement_cache_size = 256

# Maximum cached statements per pool (shared across connections)
statement_cache_pool_size = 1000

# Statement cache eviction policy: lru | lfu | fifo
statement_cache_policy = lru

# Cache statements with parameter count up to
statement_cache_max_params = 100

#------------------------------------------------------------------------------
# RESULT CACHE
#------------------------------------------------------------------------------

# Enable query result caching
result_cache_enabled = true

# Maximum memory for result cache per pool (bytes)
result_cache_size = 67108864  # 64MB

# Maximum single result size to cache (bytes)
result_cache_max_entry = 1048576  # 1MB

# Result cache TTL (seconds)
result_cache_ttl = 300

# Result cache eviction policy: lru | lfu | ttl
result_cache_policy = lru

# Tables/queries to exclude from result cache (comma-separated patterns)
result_cache_exclude = sys.*,temp_*

# Invalidate cache on DML (transaction aware)
result_cache_invalidate_on_dml = true

#------------------------------------------------------------------------------
# PRE-WARMING
#------------------------------------------------------------------------------

# Pre-warm pools on startup
prewarm = true

# Number of connections to pre-warm per pool
prewarm_count = 5

# Execute these queries to warm caches
prewarm_queries = SELECT 1

#------------------------------------------------------------------------------
# MONITORING
#------------------------------------------------------------------------------

# Enable pool statistics collection
stats_enabled = true

# Statistics collection interval (seconds)
stats_interval = 10

# Log pool statistics periodically
log_pool_stats = true

# Log slow acquisitions (milliseconds, 0 = disabled)
log_slow_acquire = 1000
```

### 4.2 Per-Database Configuration

```ini
# Database-specific pool overrides
# /etc/scratchbird/conf.d/pools.conf

[pool.main]
# Override for 'main' database
max_connections = 50
min_idle = 10
statement_cache_size = 512

[pool.analytics]
# Analytics database needs more connections for parallel queries
max_connections = 200
min_idle = 20
# Longer queries, longer timeout
acquire_timeout = 60000
# No result caching (data changes frequently)
result_cache_enabled = false

[pool.archive]
# Archive database rarely used
max_connections = 10
min_idle = 1
idle_timeout = 60
# Long-lived connections for batch jobs
max_lifetime = 86400
```

### 4.3 Per-User Configuration

```sql
-- Set pool limits for specific user
ALTER USER report_user SET pool_max_connections = 10;
ALTER USER batch_user SET pool_max_connections = 5;

-- Set pool mode for user
ALTER USER web_app SET pool_mode = 'transaction';
ALTER USER admin SET pool_mode = 'session';

-- Disable pooling for specific user
ALTER USER debug_user SET pool_enabled = false;
```

### 4.4 Application-Level Configuration

Connection string parameters:

```
# Native protocol
scratchbird://user:pass@host:3092/db?pool_mode=transaction&min_idle=5

# PostgreSQL protocol
postgresql://user:pass@host:5432/db?application_name=myapp&pool_size=20
```

---

## 5. Connection Management

### 5.1 Connection Factory

```c
class ConnectionFactory {
public:
    StatusOr<std::unique_ptr<Connection>> create(
        const ConnectionConfig& config
    ) {
        auto conn = std::make_unique<Connection>();

        // 1. Establish network connection
        RETURN_IF_ERROR(conn->connect(
            config.host,
            config.port,
            config.connect_timeout
        ));

        // 2. TLS handshake if required
        if (config.ssl_mode != SSLMode::DISABLE) {
            RETURN_IF_ERROR(conn->startTLS(config.ssl_config));
        }

        // 3. Protocol handshake
        RETURN_IF_ERROR(conn->handshake(config.protocol));

        // 4. Authentication
        RETURN_IF_ERROR(conn->authenticate(
            config.user,
            config.password,
            config.auth_method
        ));

        // 5. Set session defaults
        RETURN_IF_ERROR(conn->execute(
            "SET application_name = $1",
            {config.application_name}
        ));

        return conn;
    }
};
```

### 5.2 Pooled Connection Wrapper

```c
struct PooledConnection {
    // Underlying connection
    std::unique_ptr<Connection> connection;

    // Pool membership
    DatabasePool* pool;
    std::string user;
    std::string database;

    // State tracking
    ConnectionState state;
    bool in_transaction;
    bool needs_reset;
    bool is_broken;

    // Timestamps
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_used;
    std::chrono::steady_clock::time_point idle_since;
    std::chrono::steady_clock::time_point last_validated;

    // Statistics
    uint64_t queries_executed;
    uint64_t transactions_completed;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t validation_failures;

    // Statement cache reference
    StatementCache* stmt_cache;

    // Methods
    Status execute(const std::string& sql);
    Status executeWithParams(const std::string& sql, const Params& params);
    StatusOr<PreparedStatement*> prepare(const std::string& sql);
    void markBroken();
    bool isValid();
};
```

### 5.3 Connection Tagging

Connections can be tagged for routing:

```c
// Tag connection for specific workload
conn->setTag("workload", "oltp");
conn->setTag("region", "us-west");

// Acquire connection with tag requirements
auto conn = pool->acquire({
    {"workload", "oltp"},
    {"region", "us-west"}
});
```

### 5.4 Connection Affinity

For session-bound state:

```c
// Enable affinity (connection stays with client)
conn->setAffinity(client_id);

// Later acquisition gets same connection
auto conn = pool->acquireWithAffinity(client_id);

// Release affinity
conn->clearAffinity();
```

---

## 6. Statement Caching

### 6.1 Overview

Statement caching reduces parsing overhead by reusing prepared statements:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                     Statement Cache Architecture                         │
└─────────────────────────────────────────────────────────────────────────┘

                      Client Query
                           │
                           ▼
                  ┌─────────────────┐
                  │  Query Hash     │
                  │  (SHA-256)      │
                  └────────┬────────┘
                           │
              ┌────────────┴────────────┐
              │                         │
              ▼                         ▼
    ┌─────────────────┐       ┌─────────────────┐
    │ Connection-Local │       │  Pool-Shared    │
    │ Statement Cache  │       │ Statement Cache │
    │ (per-connection) │       │ (across conns)  │
    └────────┬────────┘       └────────┬────────┘
             │                         │
             │ Cache Miss              │ Cache Hit
             │                         │
             ▼                         ▼
    ┌─────────────────┐       ┌─────────────────┐
    │ Parse & Prepare │       │ Clone Statement │
    │ (full parse)    │       │ (fast path)     │
    └────────┬────────┘       └────────┬────────┘
             │                         │
             └────────────┬────────────┘
                          │
                          ▼
                  ┌─────────────────┐
                  │ Execute with    │
                  │ Parameters      │
                  └─────────────────┘
```

### 6.2 Statement Cache Entry

```c
struct CachedStatement {
    // Query identification
    std::string sql;                    // Original SQL
    uint64_t hash;                      // Query hash

    // Prepared statement
    std::string stmt_name;              // Server-side name
    std::vector<TypeOid> param_types;   // Parameter types
    std::vector<ColumnInfo> columns;    // Result columns

    // Execution plan (optional)
    std::string plan_text;              // EXPLAIN output
    std::vector<uint8_t> sblr_bytecode; // Compiled SBLR

    // Cache metadata
    std::chrono::steady_clock::time_point created;
    std::chrono::steady_clock::time_point last_used;
    uint64_t use_count;
    uint64_t total_execution_time_us;

    // Size tracking
    size_t memory_size;
};
```

### 6.3 Statement Cache Operations

```c
class StatementCache {
public:
    // Lookup statement by query hash
    CachedStatement* get(uint64_t hash) {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = cache_.find(hash);
        if (it == cache_.end()) {
            stats_.misses++;
            return nullptr;
        }

        stats_.hits++;
        it->second->last_used = std::chrono::steady_clock::now();
        it->second->use_count++;

        return it->second.get();
    }

    // Add statement to cache
    void put(uint64_t hash, std::unique_ptr<CachedStatement> stmt) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Evict if necessary
        while (cache_.size() >= max_size_ ||
               current_memory_ + stmt->memory_size > max_memory_) {
            evictOne();
        }

        current_memory_ += stmt->memory_size;
        cache_[hash] = std::move(stmt);
        stats_.inserts++;
    }

    // Remove statement from cache
    void remove(uint64_t hash) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        auto it = cache_.find(hash);
        if (it != cache_.end()) {
            current_memory_ -= it->second->memory_size;
            cache_.erase(it);
            stats_.evictions++;
        }
    }

    // Invalidate statements for table
    void invalidateForTable(const std::string& table_name) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        for (auto it = cache_.begin(); it != cache_.end(); ) {
            if (it->second->references_table(table_name)) {
                current_memory_ -= it->second->memory_size;
                it = cache_.erase(it);
                stats_.invalidations++;
            } else {
                ++it;
            }
        }
    }

private:
    void evictOne() {
        // LRU eviction
        auto oldest = std::min_element(
            cache_.begin(), cache_.end(),
            [](const auto& a, const auto& b) {
                return a.second->last_used < b.second->last_used;
            }
        );

        if (oldest != cache_.end()) {
            current_memory_ -= oldest->second->memory_size;
            cache_.erase(oldest);
            stats_.evictions++;
        }
    }

    std::shared_mutex mutex_;
    std::unordered_map<uint64_t, std::unique_ptr<CachedStatement>> cache_;
    size_t max_size_;
    size_t max_memory_;
    size_t current_memory_ = 0;
    StatementCacheStats stats_;
};
```

### 6.4 Query Normalization

For effective caching, queries are normalized:

```c
// Original queries (different literals)
"SELECT * FROM users WHERE id = 1"
"SELECT * FROM users WHERE id = 42"
"SELECT * FROM users WHERE id = 999"

// Normalized (same cache key)
"SELECT * FROM users WHERE id = $1"

// Normalization rules:
// 1. Replace numeric literals with $N parameters
// 2. Replace string literals with $N parameters
// 3. Normalize whitespace
// 4. Convert keywords to uppercase
// 5. Remove comments
```

```c
class QueryNormalizer {
public:
    struct NormalizedQuery {
        std::string sql;              // Normalized SQL
        uint64_t hash;                // Hash of normalized SQL
        std::vector<Value> literals;  // Extracted literals
    };

    NormalizedQuery normalize(const std::string& sql) {
        NormalizedQuery result;
        std::string normalized;
        int param_index = 1;

        Lexer lexer(sql);
        while (auto token = lexer.next()) {
            switch (token.type) {
                case TokenType::INTEGER:
                case TokenType::FLOAT:
                case TokenType::STRING:
                    // Replace literal with parameter
                    normalized += "$" + std::to_string(param_index++);
                    result.literals.push_back(token.value);
                    break;

                case TokenType::KEYWORD:
                    // Uppercase keywords
                    normalized += toUpper(token.text);
                    break;

                case TokenType::WHITESPACE:
                    // Normalize to single space
                    normalized += " ";
                    break;

                case TokenType::COMMENT:
                    // Remove comments
                    break;

                default:
                    normalized += token.text;
                    break;
            }
        }

        result.sql = normalized;
        result.hash = hashQuery(normalized);
        return result;
    }
};
```

---

## 7. Result Caching

### 7.1 Overview

Result caching stores query results for fast retrieval:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      Result Cache Architecture                           │
└─────────────────────────────────────────────────────────────────────────┘

                       Query Request
                            │
                            ▼
                   ┌────────────────┐
                   │ Cache Key      │
                   │ Generation     │
                   │ (query + params│
                   │  + txn_epoch)  │
                   └───────┬────────┘
                           │
              ┌────────────┴────────────┐
              │                         │
              ▼                         ▼
    ┌─────────────────┐       ┌─────────────────┐
    │  Cache Lookup   │       │  Cache Miss     │
    │                 │       │                 │
    └────────┬────────┘       └────────┬────────┘
             │                         │
             │ Hit                     │
             ▼                         ▼
    ┌─────────────────┐       ┌─────────────────┐
    │ Validate Entry  │       │ Execute Query   │
    │ (TTL, txn_epoch)│       │                 │
    └────────┬────────┘       └────────┬────────┘
             │                         │
             │ Valid                   │ Store if cacheable
             ▼                         ▼
    ┌─────────────────┐       ┌─────────────────┐
    │ Return Cached   │       │ Cache Result    │
    │ Result          │       │ (if size OK)    │
    └─────────────────┘       └────────┬────────┘
                                       │
                                       ▼
                              ┌─────────────────┐
                              │ Return Result   │
                              └─────────────────┘
```

### 7.2 Cache Entry Structure

```c
struct CachedResult {
    // Cache key
    uint64_t query_hash;                // Normalized query hash
    uint64_t params_hash;               // Parameter values hash

    // Result data
    RowDescription columns;             // Column metadata
    std::vector<DataRow> rows;          // Result rows
    uint64_t row_count;

    // Validity tracking
    uint64_t txn_epoch;                 // MGA epoch when cached
    std::chrono::steady_clock::time_point created;
    std::chrono::steady_clock::time_point expires;

    // Dependency tracking (for invalidation)
    std::set<std::string> tables;       // Tables referenced
    std::set<uint64_t> row_ids;         // Specific rows (for fine-grained)

    // Statistics
    uint64_t hit_count;
    size_t memory_size;

    // Check if still valid
    bool isValid(uint64_t current_epoch) const {
        // Check TTL
        if (std::chrono::steady_clock::now() > expires) {
            return false;
        }

        // Check MGA epoch (data may have changed)
        if (current_epoch > txn_epoch) {
            // Need to verify tables haven't been modified
            return false;
        }

        return true;
    }
};
```

### 7.3 Result Cache Implementation

```c
class ResultCache {
public:
    // Query result cache lookup
    CachedResult* get(
        uint64_t query_hash,
        uint64_t params_hash,
        uint64_t current_epoch
    ) {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        CacheKey key{query_hash, params_hash};
        auto it = cache_.find(key);

        if (it == cache_.end()) {
            stats_.misses++;
            return nullptr;
        }

        CachedResult* entry = it->second.get();

        // Validate entry
        if (!entry->isValid(current_epoch)) {
            stats_.stale_hits++;
            return nullptr;
        }

        stats_.hits++;
        entry->hit_count++;
        return entry;
    }

    // Store query result
    void put(
        uint64_t query_hash,
        uint64_t params_hash,
        std::unique_ptr<CachedResult> result
    ) {
        // Check if result is cacheable
        if (result->memory_size > max_entry_size_) {
            stats_.too_large++;
            return;
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Evict if necessary
        while (current_memory_ + result->memory_size > max_memory_) {
            evictOne();
        }

        CacheKey key{query_hash, params_hash};
        current_memory_ += result->memory_size;
        cache_[key] = std::move(result);

        // Track table dependencies
        for (const auto& table : result->tables) {
            table_index_[table].insert(key);
        }

        stats_.inserts++;
    }

    // Invalidate results for table
    void invalidateTable(const std::string& table_name) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        auto it = table_index_.find(table_name);
        if (it == table_index_.end()) {
            return;
        }

        for (const auto& key : it->second) {
            auto cache_it = cache_.find(key);
            if (cache_it != cache_.end()) {
                current_memory_ -= cache_it->second->memory_size;
                cache_.erase(cache_it);
                stats_.invalidations++;
            }
        }

        table_index_.erase(it);
    }

    // Invalidate all results
    void clear() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cache_.clear();
        table_index_.clear();
        current_memory_ = 0;
        stats_.clears++;
    }

private:
    struct CacheKey {
        uint64_t query_hash;
        uint64_t params_hash;

        bool operator==(const CacheKey& other) const {
            return query_hash == other.query_hash &&
                   params_hash == other.params_hash;
        }
    };

    struct CacheKeyHash {
        size_t operator()(const CacheKey& key) const {
            return key.query_hash ^ (key.params_hash << 1);
        }
    };

    std::shared_mutex mutex_;
    std::unordered_map<CacheKey, std::unique_ptr<CachedResult>, CacheKeyHash> cache_;
    std::unordered_map<std::string, std::set<CacheKey>> table_index_;
    size_t max_memory_;
    size_t max_entry_size_;
    size_t current_memory_ = 0;
    ResultCacheStats stats_;
};
```

### 7.4 Cache Invalidation Strategies

```c
// 1. TTL-based invalidation
// Results automatically expire after configured TTL

// 2. Transaction-aware invalidation
// On COMMIT of DML:
void TransactionManager::onCommit(Transaction* txn) {
    for (const auto& table : txn->modified_tables) {
        result_cache_->invalidateTable(table);
    }
}

// 3. Fine-grained row-level invalidation
// Track specific rows in cache entries
void ResultCache::invalidateRow(const std::string& table, uint64_t row_id) {
    // Only invalidate entries that reference this specific row
}

// 4. Schema change invalidation
// On DDL:
void SchemaManager::onDDL(const std::string& table) {
    statement_cache_->invalidateForTable(table);
    result_cache_->invalidateTable(table);
}
```

---

## 8. Health Checking

### 8.1 Health Check Types

| Type | When | Query | Action on Failure |
|------|------|-------|-------------------|
| **Acquire** | Before giving to client | `SELECT 1` | Remove from pool |
| **Release** | When returned to pool | Quick validation | Mark for revalidation |
| **Background** | Periodically | `SELECT 1` | Remove if consecutive failures |
| **Heartbeat** | Idle connections | TCP keepalive | Close if unresponsive |

### 8.2 Health Checker Implementation

```c
class HealthChecker {
public:
    void start() {
        running_ = true;
        thread_ = std::thread(&HealthChecker::run, this);
    }

    void stop() {
        running_ = false;
        cv_.notify_all();
        thread_.join();
    }

private:
    void run() {
        while (running_) {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, config_.validation_interval);

            if (!running_) break;

            validateAllConnections();
        }
    }

    void validateAllConnections() {
        for (auto& [name, pool] : pool_manager_->pools()) {
            auto idle_connections = pool->getIdleConnections();

            for (auto* conn : idle_connections) {
                if (!validateConnection(conn)) {
                    conn->validation_failures++;

                    if (conn->validation_failures >= config_.max_failures) {
                        pool->removeConnection(conn);
                        stats_.removed++;
                    }
                } else {
                    conn->validation_failures = 0;
                    conn->last_validated = std::chrono::steady_clock::now();
                    stats_.validated++;
                }
            }
        }
    }

    bool validateConnection(PooledConnection* conn) {
        try {
            // Set short timeout for validation
            conn->connection->setTimeout(config_.validation_timeout);

            // Execute validation query
            auto result = conn->execute(config_.validation_query);

            // Reset timeout
            conn->connection->setTimeout(0);

            return result.ok();
        } catch (...) {
            return false;
        }
    }

    PoolManager* pool_manager_;
    HealthCheckConfig config_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    HealthCheckStats stats_;
};
```

### 8.3 Connection Validation Query

```sql
-- Default validation query
SELECT 1

-- Extended validation (checks database health)
SELECT
    pg_is_in_recovery() AS is_standby,
    pg_postmaster_start_time() AS start_time,
    CASE WHEN pg_is_in_recovery()
         THEN pg_last_wal_replay_lsn()::text
         ELSE pg_current_wal_lsn()::text
    END AS lsn

-- Full validation (checks all subsystems)
SELECT
    (SELECT COUNT(*) FROM pg_stat_activity) AS connections,
    (SELECT COUNT(*) FROM pg_locks WHERE NOT granted) AS waiting_locks,
    (SELECT COALESCE(SUM(blks_hit) * 100.0 / NULLIF(SUM(blks_hit) + SUM(blks_read), 0), 0)
     FROM pg_stat_database) AS cache_hit_ratio
```

---

## 9. Load Balancing

### 9.1 Load Balancing Strategies

For multi-node deployments (Beta phase):

| Strategy | Description | Use Case |
|----------|-------------|----------|
| **Round Robin** | Distribute evenly | Homogeneous nodes |
| **Least Connections** | Send to least busy | Variable query complexity |
| **Weighted** | Based on node capacity | Heterogeneous nodes |
| **Latency-Based** | Send to fastest | Geographically distributed |
| **Read/Write Split** | Writes to primary, reads to replicas | Read-heavy workloads |

### 9.2 Node Health Tracking

```c
struct NodeHealth {
    std::string host;
    uint16_t port;

    // Health metrics
    bool is_primary;
    bool is_healthy;
    uint32_t active_connections;
    uint32_t pending_requests;

    // Latency tracking
    std::chrono::microseconds avg_latency;
    std::chrono::microseconds p99_latency;

    // Replication status (for replicas)
    uint64_t replay_lag_bytes;
    std::chrono::seconds replay_lag_seconds;

    // Failure tracking
    uint32_t consecutive_failures;
    std::chrono::steady_clock::time_point last_failure;
    std::chrono::steady_clock::time_point last_success;
};
```

### 9.3 Connection Routing

```c
PooledConnection* LoadBalancer::route(const QueryContext& ctx) {
    std::vector<NodeHealth*> candidates;

    // Filter healthy nodes
    for (auto& node : nodes_) {
        if (!node.is_healthy) continue;

        // Write queries go to primary only
        if (ctx.is_write && !node.is_primary) continue;

        // Check replication lag for read queries
        if (!ctx.is_write && node.replay_lag_seconds > max_lag_) continue;

        candidates.push_back(&node);
    }

    if (candidates.empty()) {
        return nullptr;  // No healthy nodes
    }

    // Select based on strategy
    NodeHealth* selected = nullptr;
    switch (strategy_) {
        case Strategy::ROUND_ROBIN:
            selected = candidates[next_index_++ % candidates.size()];
            break;

        case Strategy::LEAST_CONNECTIONS:
            selected = *std::min_element(candidates.begin(), candidates.end(),
                [](auto* a, auto* b) {
                    return a->active_connections < b->active_connections;
                });
            break;

        case Strategy::LATENCY:
            selected = *std::min_element(candidates.begin(), candidates.end(),
                [](auto* a, auto* b) {
                    return a->avg_latency < b->avg_latency;
                });
            break;
    }

    return pools_[selected->host]->acquire(ctx.user, ctx.timeout);
}
```

---

## 10. Statistics and Monitoring

### 10.1 Pool Statistics

```c
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
    std::atomic<uint64_t> total_acquire_time{0};
    std::atomic<uint64_t> total_wait_time{0};
    std::atomic<uint64_t> max_acquire_time{0};
    std::atomic<uint64_t> max_wait_time{0};

    // Cache statistics
    StatementCacheStats stmt_cache;
    ResultCacheStats result_cache;

    // Health check statistics
    std::atomic<uint64_t> validations{0};
    std::atomic<uint64_t> validation_failures{0};
    std::atomic<uint64_t> evictions{0};

    // Calculated metrics
    double getAcquireRate() const {
        return acquires / uptime_seconds();
    }

    double getAvgAcquireTime() const {
        return acquires > 0 ? total_acquire_time / acquires : 0;
    }

    double getCacheHitRatio() const {
        auto total = stmt_cache.hits + stmt_cache.misses;
        return total > 0 ? (double)stmt_cache.hits / total : 0;
    }
};
```

### 10.2 Prometheus Metrics

```prometheus
# Pool metrics
scratchbird_pool_connections_active{database="main",user="all"} 42
scratchbird_pool_connections_idle{database="main",user="all"} 8
scratchbird_pool_connections_total{database="main",user="all"} 50
scratchbird_pool_connections_max{database="main"} 100

scratchbird_pool_acquires_total{database="main"} 123456
scratchbird_pool_releases_total{database="main"} 123400
scratchbird_pool_creates_total{database="main"} 150
scratchbird_pool_closes_total{database="main"} 100
scratchbird_pool_timeouts_total{database="main"} 5
scratchbird_pool_waits_total{database="main"} 1000

# Acquisition timing
scratchbird_pool_acquire_duration_seconds_bucket{database="main",le="0.001"} 100000
scratchbird_pool_acquire_duration_seconds_bucket{database="main",le="0.01"} 120000
scratchbird_pool_acquire_duration_seconds_bucket{database="main",le="0.1"} 123000
scratchbird_pool_acquire_duration_seconds_bucket{database="main",le="1"} 123456
scratchbird_pool_acquire_duration_seconds_sum{database="main"} 1234.56
scratchbird_pool_acquire_duration_seconds_count{database="main"} 123456

# Statement cache
scratchbird_stmt_cache_hits_total{database="main"} 500000
scratchbird_stmt_cache_misses_total{database="main"} 10000
scratchbird_stmt_cache_size{database="main"} 256
scratchbird_stmt_cache_memory_bytes{database="main"} 10485760

# Result cache
scratchbird_result_cache_hits_total{database="main"} 50000
scratchbird_result_cache_misses_total{database="main"} 100000
scratchbird_result_cache_size{database="main"} 1000
scratchbird_result_cache_memory_bytes{database="main"} 67108864
scratchbird_result_cache_invalidations_total{database="main"} 5000

# Health checks
scratchbird_pool_validations_total{database="main"} 10000
scratchbird_pool_validation_failures_total{database="main"} 10
scratchbird_pool_evictions_total{database="main"} 50
```

### 10.3 Statistics Collection

```c
class StatsCollector {
public:
    void start() {
        running_ = true;
        thread_ = std::thread(&StatsCollector::run, this);
    }

private:
    void run() {
        while (running_) {
            std::this_thread::sleep_for(config_.interval);

            collectStats();

            if (config_.log_stats) {
                logStats();
            }

            if (config_.prometheus_enabled) {
                updatePrometheusMetrics();
            }
        }
    }

    void collectStats() {
        for (auto& [name, pool] : pool_manager_->pools()) {
            auto stats = pool->getStatistics();

            // Store in time series
            stats_history_[name].push_back({
                std::chrono::system_clock::now(),
                stats
            });

            // Trim old entries
            trimHistory(name);
        }
    }

    void logStats() {
        for (auto& [name, pool] : pool_manager_->pools()) {
            auto stats = pool->getStatistics();

            LOG_INFO("Pool '{}': active={}, idle={}, waits={}, "
                     "cache_hit_ratio={:.2f}%, avg_acquire={:.2f}ms",
                     name,
                     stats.active_connections.load(),
                     stats.idle_connections.load(),
                     stats.waits.load(),
                     stats.getCacheHitRatio() * 100,
                     stats.getAvgAcquireTime() / 1000.0);
        }
    }
};
```

---

## 11. SQL Interface

### 11.1 SHOW Commands

```sql
-- Show all pool status
SHOW POOL STATUS;
/*
 pool_name  | database | mode        | active | idle | waiting | max  | cache_hit_ratio
------------+----------+-------------+--------+------+---------+------+-----------------
 main       | main     | transaction | 42     | 8    | 0       | 100  | 98.50%
 production | prod     | transaction | 100    | 20   | 5       | 200  | 99.10%
 analytics  | anlyt    | session     | 50     | 10   | 0       | 100  | 95.20%
*/

-- Show specific pool
SHOW POOL STATUS FOR DATABASE main;

-- Show all connections in pool
SHOW POOL CONNECTIONS;
/*
 conn_id | database | user  | state    | created             | last_used           | queries
---------+----------+-------+----------+---------------------+---------------------+---------
 1       | main     | alice | active   | 2025-12-10 10:00:00 | 2025-12-10 14:30:00 | 1523
 2       | main     | bob   | idle     | 2025-12-10 10:05:00 | 2025-12-10 14:29:00 | 856
 3       | main     | alice | acquired | 2025-12-10 10:10:00 | 2025-12-10 14:30:01 | 2341
*/

-- Show statement cache
SHOW STATEMENT CACHE;
/*
 hash               | sql                           | hits   | last_used           | memory
--------------------+-------------------------------+--------+---------------------+--------
 0x1234567890abcdef | SELECT * FROM users WHERE ... | 50000  | 2025-12-10 14:30:00 | 4KB
 0xfedcba0987654321 | INSERT INTO logs VALUES ...   | 10000  | 2025-12-10 14:29:00 | 2KB
*/

-- Show result cache
SHOW RESULT CACHE;
/*
 query_hash         | params_hash        | rows | hits  | created             | expires             | memory
--------------------+--------------------+------+-------+---------------------+---------------------+--------
 0x1234567890abcdef | 0xaaaaaaaaaaaaaaaa | 100  | 5000  | 2025-12-10 14:25:00 | 2025-12-10 14:30:00 | 50KB
*/

-- Show pool statistics
SHOW POOL STATISTICS;
/*
 metric                  | value
-------------------------+-------------
 total_acquires          | 1234567
 total_releases          | 1234500
 total_creates           | 500
 total_closes            | 400
 total_timeouts          | 10
 avg_acquire_time_ms     | 0.5
 max_acquire_time_ms     | 50
 stmt_cache_hit_ratio    | 98.5%
 result_cache_hit_ratio  | 45.2%
 validations             | 10000
 validation_failures     | 5
*/
```

### 11.2 Management Commands

```sql
-- Clear statement cache
DISCARD STATEMENT CACHE;
DISCARD STATEMENT CACHE FOR DATABASE main;

-- Clear result cache
DISCARD RESULT CACHE;
DISCARD RESULT CACHE FOR DATABASE main;
DISCARD RESULT CACHE FOR TABLE users;

-- Force pool drain (for maintenance)
ALTER POOL main DRAIN;

-- Resume pool
ALTER POOL main RESUME;

-- Resize pool
ALTER POOL main SET max_connections = 200;
ALTER POOL main SET min_idle = 20;

-- Reset pool statistics
RESET POOL STATISTICS;
RESET POOL STATISTICS FOR DATABASE main;
```

### 11.3 Hints

```sql
-- Bypass statement cache
SELECT /*+ NO_STMT_CACHE */ * FROM users WHERE id = 1;

-- Bypass result cache
SELECT /*+ NO_RESULT_CACHE */ * FROM users WHERE id = 1;

-- Force cache refresh
SELECT /*+ REFRESH_CACHE */ * FROM users WHERE id = 1;

-- Set cache TTL for this query
SELECT /*+ CACHE_TTL(60) */ * FROM config WHERE key = 'setting';

-- Disable pooling for this connection
SET pool_enabled = false;

-- Set pool mode for session
SET pool_mode = 'session';
```

---

## 12. Implementation Details

### 12.1 Thread Safety

```c
// Pool uses multiple synchronization primitives

class DatabasePool {
private:
    // Main pool lock (for connection list operations)
    std::mutex pool_mutex_;

    // Condition variable for waiters
    std::condition_variable available_;

    // Read-write lock for statement cache
    std::shared_mutex stmt_cache_mutex_;

    // Read-write lock for result cache
    std::shared_mutex result_cache_mutex_;

    // Atomic counters for statistics
    std::atomic<uint64_t> active_count_{0};
    std::atomic<uint64_t> idle_count_{0};

    // Lock-free queue for connection returns
    moodycamel::ConcurrentQueue<PooledConnection*> return_queue_;
};
```

### 12.2 Memory Management

```c
// Connection memory pools
class ConnectionMemoryPool {
public:
    // Pre-allocate connection structures
    void initialize(size_t count) {
        connections_.reserve(count);
        for (size_t i = 0; i < count; i++) {
            connections_.push_back(std::make_unique<PooledConnection>());
            free_list_.push(connections_.back().get());
        }
    }

    PooledConnection* allocate() {
        PooledConnection* conn;
        if (free_list_.try_pop(conn)) {
            return conn;
        }

        // Grow pool if needed
        std::lock_guard<std::mutex> lock(grow_mutex_);
        auto new_conn = std::make_unique<PooledConnection>();
        auto* ptr = new_conn.get();
        connections_.push_back(std::move(new_conn));
        return ptr;
    }

    void deallocate(PooledConnection* conn) {
        conn->reset();
        free_list_.push(conn);
    }

private:
    std::vector<std::unique_ptr<PooledConnection>> connections_;
    moodycamel::ConcurrentQueue<PooledConnection*> free_list_;
    std::mutex grow_mutex_;
};
```

### 12.3 Connection Reset Protocol

```c
// Fast reset for transaction pool mode
void PooledConnection::fastReset() {
    // Only reset if dirty
    if (!needs_reset_) return;

    // Batch reset commands
    connection_->executeMultiple({
        "ROLLBACK",
        "RESET ALL",
        "DISCARD TEMP",
        "SET search_path TO DEFAULT"
    });

    needs_reset_ = false;
}

// Full reset for session pool mode
void PooledConnection::fullReset() {
    connection_->executeMultiple({
        "ROLLBACK",
        "RESET ALL",
        "DISCARD ALL",
        "SET search_path TO DEFAULT",
        "DEALLOCATE ALL",
        "UNLISTEN *",
        "SELECT pg_advisory_unlock_all()"
    });

    needs_reset_ = false;
}
```

---

## 13. Security Considerations

### 13.1 Connection Isolation

```c
// Ensure connections don't leak state between users
void DatabasePool::releaseConnection(PooledConnection* conn, const std::string& user) {
    // Always reset when user changes
    if (conn->user != user) {
        conn->needs_reset = true;
    }

    // Reset security context
    if (conn->security_context_changed) {
        conn->execute("RESET ROLE");
        conn->execute("RESET SESSION AUTHORIZATION");
        conn->security_context_changed = false;
    }

    release(conn);
}
```

### 13.2 Credential Handling

```c
// Never log credentials
void ConnectionFactory::create(const ConnectionConfig& config) {
    LOG_DEBUG("Creating connection to {}:{} as user '{}'",
              config.host, config.port, config.user);
    // Note: password is NOT logged
}

// Secure password storage
struct ConnectionConfig {
    std::string user;
    SecureString password;  // Zeroed on destruction

    ~ConnectionConfig() {
        password.clear();  // Secure wipe
    }
};
```

### 13.3 Cache Security

```c
// Result cache respects row-level security
CachedResult* ResultCache::get(
    uint64_t query_hash,
    uint64_t params_hash,
    uint64_t current_epoch,
    const SecurityContext& ctx  // Current user's security context
) {
    auto* entry = getEntry(query_hash, params_hash);
    if (!entry) return nullptr;

    // Cache entry was created with different security context
    if (entry->security_context != ctx) {
        return nullptr;  // Force re-execution with current context
    }

    return entry;
}

// Include security context in cache key for RLS tables
uint64_t ResultCache::computeKey(
    const std::string& query,
    const Params& params,
    const SecurityContext& ctx,
    const std::set<std::string>& rls_tables
) {
    uint64_t hash = hashQuery(query);
    hash = combineHash(hash, hashParams(params));

    // Include security context if query touches RLS tables
    if (!rls_tables.empty()) {
        hash = combineHash(hash, ctx.user_id);
        hash = combineHash(hash, ctx.role_id);
    }

    return hash;
}
```

---

## 14. Troubleshooting

### 14.1 Common Issues

**Connection pool exhausted:**
```sql
-- Check current usage
SHOW POOL STATUS;

-- Check what's holding connections
SHOW POOL CONNECTIONS WHERE state = 'acquired';

-- Check for long-running queries
SELECT * FROM pg_stat_activity
WHERE state = 'active'
AND query_start < NOW() - INTERVAL '5 minutes';

-- Solutions:
-- 1. Increase max_connections
ALTER POOL main SET max_connections = 200;

-- 2. Reduce connection hold time (use transaction mode)
SET pool_mode = 'transaction';

-- 3. Fix application connection leaks
```

**High cache miss rate:**
```sql
-- Check cache statistics
SHOW STATEMENT CACHE;
SHOW RESULT CACHE;

-- Solutions:
-- 1. Increase cache size
ALTER SYSTEM SET pool.statement_cache_size = 1000;

-- 2. Check for non-parameterized queries
-- Look for queries that differ only in literal values

-- 3. Disable result cache for frequently changing tables
ALTER TABLE hot_table SET (result_cache = false);
```

**Connection validation failures:**
```sql
-- Check validation statistics
SHOW POOL STATISTICS;

-- Check connection health
SELECT * FROM pg_stat_activity WHERE state = 'idle';

-- Solutions:
-- 1. Check network connectivity
-- 2. Increase validation_timeout
-- 3. Check database server health
```

### 14.2 Debug Mode

```ini
[pool]
# Enable debug logging
debug = true

# Log all acquisitions and releases
log_acquire_release = true

# Log cache operations
log_cache_operations = true

# Log validation operations
log_validations = true
```

### 14.3 Diagnostic Queries

```sql
-- Pool health overview
SELECT
    database,
    active_connections,
    idle_connections,
    waiting_requests,
    ROUND(100.0 * active_connections / max_connections, 1) AS utilization_pct,
    ROUND(stmt_cache_hit_ratio * 100, 1) AS stmt_cache_hit_pct,
    ROUND(result_cache_hit_ratio * 100, 1) AS result_cache_hit_pct
FROM scratchbird_pool_status;

-- Connection age distribution
SELECT
    database,
    COUNT(*) FILTER (WHERE age < INTERVAL '1 minute') AS under_1min,
    COUNT(*) FILTER (WHERE age BETWEEN INTERVAL '1 minute' AND INTERVAL '10 minutes') AS 1_to_10min,
    COUNT(*) FILTER (WHERE age BETWEEN INTERVAL '10 minutes' AND INTERVAL '1 hour') AS 10min_to_1hr,
    COUNT(*) FILTER (WHERE age > INTERVAL '1 hour') AS over_1hr
FROM scratchbird_pool_connections
GROUP BY database;

-- Top cached statements by hit count
SELECT
    LEFT(sql, 50) AS sql_preview,
    hits,
    misses,
    ROUND(100.0 * hits / (hits + misses), 1) AS hit_ratio
FROM scratchbird_stmt_cache
ORDER BY hits DESC
LIMIT 10;
```

---

## Appendix A: Configuration Quick Reference

| Setting | Default | Range | Hot Reload |
|---------|---------|-------|------------|
| `pool.enabled` | true | boolean | No |
| `pool.mode` | transaction | session/transaction/statement | Yes |
| `pool.min_idle` | 5 | 0-1000 | Yes |
| `pool.max_idle` | 20 | 1-10000 | Yes |
| `pool.max_connections` | 100 | 1-10000 | Yes |
| `pool.acquire_timeout` | 30000 | 1-600000 ms | Yes |
| `pool.idle_timeout` | 300 | 0-86400 s | Yes |
| `pool.max_lifetime` | 3600 | 0-86400 s | Yes |
| `pool.validation_interval` | 60 | 0-3600 s | Yes |
| `pool.statement_cache_enabled` | true | boolean | No |
| `pool.statement_cache_size` | 256 | 0-10000 | Yes |
| `pool.result_cache_enabled` | true | boolean | No |
| `pool.result_cache_size` | 64MB | 0-10GB | Yes |
| `pool.result_cache_ttl` | 300 | 0-86400 s | Yes |

---

## Appendix B: Comparison with External Poolers

| Feature | ScratchBird Built-in | pgBouncer | ProxySQL |
|---------|---------------------|-----------|----------|
| Zero network hop | ✅ | ❌ | ❌ |
| Statement caching | ✅ | ❌ | ✅ |
| Result caching | ✅ | ❌ | ✅ |
| MGA-aware validation | ✅ | ❌ | ❌ |
| All wire protocols | ✅ | PG only | MySQL only |
| Native type support | ✅ | ✅ | ✅ |
| Load balancing | ✅ (Beta) | ❌ | ✅ |
| Query routing | ✅ | ❌ | ✅ |
| Failover | ✅ (Beta) | ❌ | ✅ |
| Separate deployment | ❌ | ✅ | ✅ |

---

**Document End**

/**
 * @file remote_connection_pool.cpp
 * @brief Remote Connection Pool Implementation
 *
 * Part of Phase 3.7: UDR Plugin System
 */

#include "scratchbird/fdw/remote_connection_pool.h"

#include <algorithm>
#include <sstream>

namespace scratchbird {
namespace fdw {

// =============================================================================
// UserPool Implementation
// =============================================================================

UserPool::UserPool(ServerPool* parent,
                   const std::string& local_user,
                   const UserMapping& mapping)
    : parent_(parent)
    , local_user_(local_user)
    , mapping_(mapping)
    , waiting_count_(0)
{
    stats_.local_user = local_user;
}

UserPool::~UserPool() {
    closeAll();
}

Result<std::unique_ptr<IProtocolAdapter>> UserPool::acquire(
    std::chrono::milliseconds timeout)
{
    std::unique_lock lock(mutex_);
    auto deadline = std::chrono::steady_clock::now() + timeout;
    auto acquire_start = std::chrono::steady_clock::now();

    while (true) {
        // Try to get idle connection
        if (!idle_queue_.empty()) {
            size_t idx = idle_queue_.front();
            idle_queue_.pop();

            auto& conn = connections_[idx];
            conn.state = ConnectionState::EXECUTING;
            conn.last_used = std::chrono::steady_clock::now();
            conn.use_count++;

            stats_.total_acquires++;

            // Calculate wait time
            auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - acquire_start);
            stats_.avg_acquire_wait_ms =
                (stats_.avg_acquire_wait_ms * (stats_.total_acquires - 1) +
                 wait_time.count()) / stats_.total_acquires;

            return Result<std::unique_ptr<IProtocolAdapter>>(std::move(conn.adapter));
        }

        // Can we create new connection?
        if (connections_.size() < parent_->getServerDefinition().max_connections) {
            lock.unlock();
            auto result = createConnection();
            if (!result) {
                return makeError<std::unique_ptr<IProtocolAdapter>>(
                    result.errorCode(), result.errorMessage());
            }

            lock.lock();
            connections_.push_back(std::move(*result));
            auto& conn = connections_.back();
            conn.state = ConnectionState::EXECUTING;
            conn.use_count = 1;

            stats_.total_acquires++;

            auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - acquire_start);
            stats_.avg_acquire_wait_ms =
                (stats_.avg_acquire_wait_ms * (stats_.total_acquires - 1) +
                 wait_time.count()) / stats_.total_acquires;

            return Result<std::unique_ptr<IProtocolAdapter>>(std::move(conn.adapter));
        }

        // Wait for available connection
        waiting_count_++;
        stats_.waiting_requests = waiting_count_;

        if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            waiting_count_--;
            stats_.waiting_requests = waiting_count_;
            stats_.acquire_timeouts++;
            return makeError<std::unique_ptr<IProtocolAdapter>>(
                core::Status::LOCK_TIMEOUT,
                "Timeout waiting for remote connection");
        }
        waiting_count_--;
        stats_.waiting_requests = waiting_count_;
    }
}

void UserPool::release(std::unique_ptr<IProtocolAdapter> adapter, bool failed) {
    std::lock_guard lock(mutex_);

    // Find the connection slot by scanning (adapter may have been moved)
    for (size_t i = 0; i < connections_.size(); ++i) {
        if (connections_[i].adapter == nullptr) {
            // This slot was the one that was acquired
            auto& pooled = connections_[i];

            if (failed || !adapter || !adapter->isConnected()) {
                // Destroy connection
                if (adapter) {
                    adapter->disconnect();
                }
                connections_.erase(connections_.begin() + i);
                stats_.connections_destroyed++;
            } else {
                // Reset connection state
                if (adapter->getState() == ConnectionState::IN_TRANSACTION) {
                    adapter->rollback();
                }
                adapter->reset();

                pooled.adapter = std::move(adapter);
                pooled.state = ConnectionState::CONNECTED;
                pooled.last_used = std::chrono::steady_clock::now();

                // Return to idle queue
                idle_queue_.push(i);
            }

            stats_.total_releases++;
            cv_.notify_one();
            return;
        }
    }

    // Adapter wasn't found in our slots, must be a new release
    // Just add it back if it's still valid
    if (!failed && adapter && adapter->isConnected()) {
        PooledConnection conn;
        conn.adapter = std::move(adapter);
        conn.state = ConnectionState::CONNECTED;
        conn.created_at = std::chrono::steady_clock::now();
        conn.last_used = conn.created_at;
        connections_.push_back(std::move(conn));
        idle_queue_.push(connections_.size() - 1);
    } else if (adapter) {
        adapter->disconnect();
        stats_.connections_destroyed++;
    }

    stats_.total_releases++;
    cv_.notify_one();
}

Result<void> UserPool::warmup(uint32_t count) {
    std::lock_guard lock(mutex_);

    for (uint32_t i = 0; i < count && connections_.size() < parent_->getServerDefinition().max_connections; ++i) {
        auto result = createConnection();
        if (!result) {
            return makeError(result.errorCode(), result.errorMessage());
        }
        connections_.push_back(std::move(*result));
        idle_queue_.push(connections_.size() - 1);
    }

    return Result<void>();
}

Result<void> UserPool::evictIdle(std::chrono::milliseconds max_idle) {
    std::lock_guard lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto min_conns = parent_->getServerDefinition().min_connections;

    std::queue<size_t> new_idle_queue;

    while (!idle_queue_.empty()) {
        size_t idx = idle_queue_.front();
        idle_queue_.pop();

        auto& conn = connections_[idx];
        auto idle_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - conn.last_used);

        if (idle_time < max_idle ||
            new_idle_queue.size() + activeCount() < min_conns) {
            new_idle_queue.push(idx);
        } else {
            conn.marked_for_eviction = true;
        }
    }

    idle_queue_ = std::move(new_idle_queue);

    // Destroy marked connections
    for (auto it = connections_.begin(); it != connections_.end(); ) {
        if (it->marked_for_eviction) {
            destroyConnection(*it);
            it = connections_.erase(it);
        } else {
            ++it;
        }
    }

    return Result<void>();
}

void UserPool::closeAll() {
    std::lock_guard lock(mutex_);

    for (auto& conn : connections_) {
        destroyConnection(conn);
    }
    connections_.clear();

    while (!idle_queue_.empty()) {
        idle_queue_.pop();
    }
}

UserPoolStats UserPool::getStats() const {
    std::lock_guard lock(mutex_);
    stats_.total_connections = static_cast<uint32_t>(connections_.size());
    stats_.idle_connections = static_cast<uint32_t>(idle_queue_.size());
    stats_.active_connections = stats_.total_connections - stats_.idle_connections;
    return stats_;
}

Result<UserPool::PooledConnection> UserPool::createConnection() {
    // Note: Called without lock held

    auto adapter = parent_->createAdapter();
    if (!adapter) {
        return makeError<PooledConnection>(core::Status::INTERNAL_ERROR,
                                            "Failed to create protocol adapter");
    }

    auto connect_result = adapter->connect(
        parent_->getServerDefinition(),
        mapping_);

    if (!connect_result) {
        return makeError<PooledConnection>(connect_result.errorCode(),
                                            connect_result.errorMessage());
    }

    PooledConnection conn;
    conn.adapter = std::move(adapter);
    conn.state = ConnectionState::CONNECTED;
    conn.created_at = std::chrono::steady_clock::now();
    conn.last_used = conn.created_at;
    conn.last_validated = conn.created_at;
    conn.use_count = 0;
    conn.query_count = 0;
    conn.marked_for_eviction = false;

    stats_.connections_created++;

    return Result<PooledConnection>(std::move(conn));
}

Result<void> UserPool::validateConnection(PooledConnection& conn) {
    auto result = conn.adapter->ping();
    if (!result || !*result) {
        stats_.validation_failures++;
        return makeError(core::Status::IO_ERROR, "Connection validation failed");
    }
    conn.last_validated = std::chrono::steady_clock::now();
    return Result<void>();
}

void UserPool::destroyConnection(PooledConnection& conn) {
    if (conn.adapter) {
        conn.adapter->disconnect();
        conn.adapter.reset();
    }
    stats_.connections_destroyed++;
}

size_t UserPool::activeCount() const {
    return connections_.size() - idle_queue_.size();
}

// =============================================================================
// ServerPool Implementation
// =============================================================================

ServerPool::ServerPool(const ServerDefinition& server)
    : server_(server)
    , healthy_(true)
    , consecutive_failures_(0)
    , created_at_(std::chrono::steady_clock::now())
    , total_queries_(0)
{
    last_healthy_ = created_at_;
}

ServerPool::~ServerPool() {
    shutdown();
}

Result<void> ServerPool::initialize() {
    // Nothing special needed - pools created on demand
    return Result<void>();
}

Result<void> ServerPool::shutdown() {
    std::unique_lock lock(user_pools_mutex_);

    for (auto& [user, pool] : user_pools_) {
        pool->closeAll();
    }
    user_pools_.clear();

    return Result<void>();
}

Result<std::unique_ptr<IProtocolAdapter>> ServerPool::acquire(
    const std::string& local_user,
    const UserMapping& mapping,
    std::chrono::milliseconds timeout)
{
    if (!healthy_.load()) {
        return makeError<std::unique_ptr<IProtocolAdapter>>(
            core::Status::CONNECTION_FAILURE,
            "Server " + server_.server_name + " is unhealthy: " + unhealthy_reason_);
    }

    UserPool* pool = getOrCreateUserPool(local_user, mapping);
    if (!pool) {
        return makeError<std::unique_ptr<IProtocolAdapter>>(
            core::Status::INTERNAL_ERROR,
            "Failed to get user pool for " + local_user);
    }

    return pool->acquire(timeout);
}

void ServerPool::release(const std::string& local_user,
                         std::unique_ptr<IProtocolAdapter> adapter,
                         bool failed)
{
    std::shared_lock lock(user_pools_mutex_);
    auto it = user_pools_.find(local_user);
    if (it != user_pools_.end()) {
        it->second->release(std::move(adapter), failed);
    }
}

void ServerPool::setMinConnections(uint32_t min) {
    server_.min_connections = min;
}

void ServerPool::setMaxConnections(uint32_t max) {
    server_.max_connections = max;
}

Result<void> ServerPool::resize(uint32_t target) {
    // Resize is handled per-user pool based on activity
    return Result<void>();
}

Result<void> ServerPool::evictIdle() {
    std::shared_lock lock(user_pools_mutex_);
    for (auto& [user, pool] : user_pools_) {
        auto result = pool->evictIdle(
            std::chrono::milliseconds(server_.idle_timeout_ms));
        if (!result) {
            return result;
        }
    }
    return Result<void>();
}

Result<void> ServerPool::validateConnection(IProtocolAdapter* conn) {
    auto result = conn->ping();
    if (!result) {
        return makeError(result.errorCode(), result.errorMessage());
    }
    if (!*result) {
        return makeError(core::Status::IO_ERROR, "Connection validation failed");
    }
    return Result<void>();
}

Result<void> ServerPool::validateAll() {
    // Validate one connection from each pool
    std::shared_lock lock(user_pools_mutex_);
    for (auto& [user, pool] : user_pools_) {
        // Validation happens during health checks
    }
    return Result<void>();
}

void ServerPool::markUnhealthy(const std::string& reason) {
    healthy_.store(false);
    unhealthy_reason_ = reason;
    consecutive_failures_++;
}

void ServerPool::markHealthy() {
    healthy_.store(true);
    unhealthy_reason_.clear();
    consecutive_failures_.store(0);
    last_healthy_ = std::chrono::steady_clock::now();
}

void ServerPool::updateDefinition(const ServerDefinition& server) {
    server_ = server;
}

ServerPoolStats ServerPool::getStats() const {
    ServerPoolStats stats;
    stats.server_name = server_.server_name;
    stats.db_type = server_.db_type;
    stats.healthy = healthy_.load();
    stats.unhealthy_reason = unhealthy_reason_;
    stats.created_at = created_at_;
    stats.last_activity = last_healthy_;

    std::shared_lock lock(user_pools_mutex_);
    stats.total_user_pools = static_cast<uint32_t>(user_pools_.size());

    for (const auto& [user, pool] : user_pools_) {
        auto user_stats = pool->getStats();
        stats.total_connections += user_stats.total_connections;
        stats.active_connections += user_stats.active_connections;
        stats.idle_connections += user_stats.idle_connections;
        stats.pending_requests += user_stats.waiting_requests;
        stats.total_acquires += user_stats.total_acquires;
        stats.total_releases += user_stats.total_releases;
        stats.acquire_timeouts += user_stats.acquire_timeouts;
    }

    stats.queries_executed = total_queries_.load();

    return stats;
}

std::unique_ptr<IProtocolAdapter> ServerPool::createAdapter() {
    return ProtocolAdapterFactory::create(server_.db_type);
}

UserPool* ServerPool::getOrCreateUserPool(const std::string& local_user,
                                           const UserMapping& mapping)
{
    // First try with read lock
    {
        std::shared_lock lock(user_pools_mutex_);
        auto it = user_pools_.find(local_user);
        if (it != user_pools_.end()) {
            return it->second.get();
        }
    }

    // Need to create - acquire write lock
    std::unique_lock lock(user_pools_mutex_);

    // Double-check after acquiring write lock
    auto it = user_pools_.find(local_user);
    if (it != user_pools_.end()) {
        return it->second.get();
    }

    auto pool = std::make_unique<UserPool>(this, local_user, mapping);
    auto* ptr = pool.get();
    user_pools_[local_user] = std::move(pool);
    return ptr;
}

// =============================================================================
// PooledRemoteConnection Implementation
// =============================================================================

PooledRemoteConnection::PooledRemoteConnection(
    std::unique_ptr<IProtocolAdapter> adapter,
    RemoteConnectionPoolRegistry* registry,
    const std::string& server_name,
    const std::string& local_user)
    : adapter_(std::move(adapter))
    , registry_(registry)
    , server_name_(server_name)
    , local_user_(local_user)
    , failed_(false)
    , released_(false)
{
}

PooledRemoteConnection::PooledRemoteConnection(PooledRemoteConnection&& other) noexcept
    : adapter_(std::move(other.adapter_))
    , registry_(other.registry_)
    , server_name_(std::move(other.server_name_))
    , local_user_(std::move(other.local_user_))
    , failed_(other.failed_)
    , released_(other.released_)
{
    other.registry_ = nullptr;
    other.released_ = true;
}

PooledRemoteConnection& PooledRemoteConnection::operator=(PooledRemoteConnection&& other) noexcept {
    if (this != &other) {
        release();

        adapter_ = std::move(other.adapter_);
        registry_ = other.registry_;
        server_name_ = std::move(other.server_name_);
        local_user_ = std::move(other.local_user_);
        failed_ = other.failed_;
        released_ = other.released_;

        other.registry_ = nullptr;
        other.released_ = true;
    }
    return *this;
}

PooledRemoteConnection::~PooledRemoteConnection() {
    release();
}

void PooledRemoteConnection::release() {
    if (!released_ && registry_ && adapter_) {
        auto* pool = registry_->getServerPool(server_name_);
        if (pool) {
            pool->release(local_user_, std::move(adapter_), failed_);
        }
        released_ = true;
    }
}

// =============================================================================
// HealthChecker Implementation
// =============================================================================

HealthChecker::HealthChecker(RemoteConnectionPoolRegistry* registry)
    : registry_(registry)
    , running_(false)
{
}

HealthChecker::~HealthChecker() {
    stop();
}

void HealthChecker::start() {
    if (running_.exchange(true)) {
        return;  // Already running
    }

    thread_ = std::thread(&HealthChecker::runLoop, this);
}

void HealthChecker::stop() {
    if (!running_.exchange(false)) {
        return;  // Not running
    }

    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void HealthChecker::checkNow(const std::string& server_name) {
    std::lock_guard lock(mutex_);
    immediate_check_server_ = server_name;
    cv_.notify_one();
}

void HealthChecker::runLoop() {
    while (running_.load()) {
        std::unique_lock lock(mutex_);

        // Wait for interval or notification
        cv_.wait_for(lock, config_.interval, [this] {
            return !running_.load() || !immediate_check_server_.empty();
        });

        if (!running_.load()) {
            break;
        }

        // Check immediate server if requested
        std::string immediate_server;
        if (!immediate_check_server_.empty()) {
            immediate_server = immediate_check_server_;
            immediate_check_server_.clear();
        }
        lock.unlock();

        if (!immediate_server.empty()) {
            if (auto* pool = registry_->getServerPool(immediate_server)) {
                checkServer(pool);
            }
        } else {
            // Check all servers
            auto servers = registry_->listServers();
            for (const auto& name : servers) {
                if (!running_.load()) break;
                if (auto* pool = registry_->getServerPool(name)) {
                    checkServer(pool);
                }
            }
        }
    }
}

void HealthChecker::checkServer(ServerPool* pool) {
    try {
        // Create a test connection and ping
        auto adapter = pool->createAdapter();
        if (!adapter) {
            pool->markUnhealthy("Failed to create adapter");
            return;
        }

        const auto& server = pool->getServerDefinition();

        // Try to find a user mapping - use first available for health check
        // In production, health checks might use a dedicated health check user
        UserMapping mapping;
        mapping.remote_user = "";  // Will need proper mapping

        auto connect_result = adapter->connect(server, mapping);
        if (!connect_result) {
            pool->markUnhealthy("Connection failed: " + connect_result.errorMessage());
            return;
        }

        auto ping_result = adapter->ping();
        if (!ping_result || !*ping_result) {
            pool->markUnhealthy("Ping failed");
            adapter->disconnect();
            return;
        }

        adapter->disconnect();
        pool->markHealthy();

    } catch (const std::exception& e) {
        pool->markUnhealthy(std::string("Exception: ") + e.what());
    }
}

void HealthChecker::checkConnection(IProtocolAdapter* conn) {
    auto result = conn->ping();
    // Individual connection check - handled by pool
}

// =============================================================================
// RemoteConnectionPoolRegistry Implementation
// =============================================================================

RemoteConnectionPoolRegistry& RemoteConnectionPoolRegistry::instance() {
    static RemoteConnectionPoolRegistry instance;
    return instance;
}

RemoteConnectionPoolRegistry::RemoteConnectionPoolRegistry()
    : initialized_(false)
{
}

RemoteConnectionPoolRegistry::~RemoteConnectionPoolRegistry() {
    shutdown();
}

void RemoteConnectionPoolRegistry::initialize() {
    if (initialized_.exchange(true)) {
        return;
    }

    health_checker_ = std::make_unique<HealthChecker>(this);
}

void RemoteConnectionPoolRegistry::shutdown() {
    if (!initialized_.exchange(false)) {
        return;
    }

    stopHealthChecker();

    std::unique_lock lock(server_pools_mutex_);
    for (auto& [name, pool] : server_pools_) {
        pool->shutdown();
    }
    server_pools_.clear();
}

Result<void> RemoteConnectionPoolRegistry::registerServer(const ServerDefinition& server) {
    std::unique_lock lock(server_pools_mutex_);

    if (server_pools_.find(server.server_name) != server_pools_.end()) {
        return makeError(core::Status::DUPLICATE_OBJECT,
                         "Server already registered: " + server.server_name);
    }

    auto pool = std::make_unique<ServerPool>(server);
    auto init_result = pool->initialize();
    if (!init_result) {
        return init_result;
    }

    server_pools_[server.server_name] = std::move(pool);
    return Result<void>();
}

Result<void> RemoteConnectionPoolRegistry::unregisterServer(const std::string& server_name) {
    std::unique_lock lock(server_pools_mutex_);

    auto it = server_pools_.find(server_name);
    if (it == server_pools_.end()) {
        return makeError(core::Status::NOT_FOUND,
                         "Server not found: " + server_name);
    }

    it->second->shutdown();
    server_pools_.erase(it);
    return Result<void>();
}

Result<void> RemoteConnectionPoolRegistry::updateServer(const ServerDefinition& server) {
    std::shared_lock lock(server_pools_mutex_);

    auto it = server_pools_.find(server.server_name);
    if (it == server_pools_.end()) {
        return makeError(core::Status::NOT_FOUND,
                         "Server not found: " + server.server_name);
    }

    it->second->updateDefinition(server);
    return Result<void>();
}

const ServerDefinition* RemoteConnectionPoolRegistry::getServer(
    const std::string& server_name) const
{
    std::shared_lock lock(server_pools_mutex_);

    auto it = server_pools_.find(server_name);
    if (it == server_pools_.end()) {
        return nullptr;
    }

    return &it->second->getServerDefinition();
}

std::vector<std::string> RemoteConnectionPoolRegistry::listServers() const {
    std::shared_lock lock(server_pools_mutex_);

    std::vector<std::string> result;
    result.reserve(server_pools_.size());

    for (const auto& [name, pool] : server_pools_) {
        result.push_back(name);
    }

    return result;
}

Result<PooledRemoteConnection> RemoteConnectionPoolRegistry::acquire(
    const std::string& server_name,
    const std::string& local_user,
    std::chrono::milliseconds timeout)
{
    ServerPool* pool = nullptr;
    {
        std::shared_lock lock(server_pools_mutex_);
        auto it = server_pools_.find(server_name);
        if (it == server_pools_.end()) {
            return makeError<PooledRemoteConnection>(
                core::Status::NOT_FOUND,
                "Server not found: " + server_name);
        }
        pool = it->second.get();
    }

    // Find user mapping
    const ServerDefinition& server = pool->getServerDefinition();
    const UserMapping* mapping = findUserMapping(server.server_id, local_user);
    if (!mapping) {
        // Try PUBLIC mapping
        mapping = findUserMapping(server.server_id, "PUBLIC");
    }
    if (!mapping) {
        return makeError<PooledRemoteConnection>(
            core::Status::NOT_FOUND,
            "No user mapping found for " + local_user + " on server " + server_name);
    }

    auto adapter_result = pool->acquire(local_user, *mapping, timeout);
    if (!adapter_result) {
        return makeError<PooledRemoteConnection>(
            adapter_result.errorCode(),
            adapter_result.errorMessage());
    }

    return Result<PooledRemoteConnection>(PooledRemoteConnection(
        std::move(*adapter_result),
        this,
        server_name,
        local_user));
}

void RemoteConnectionPoolRegistry::release(PooledRemoteConnection&& conn) {
    conn.release();
}

Result<void> RemoteConnectionPoolRegistry::addUserMapping(const UserMapping& mapping) {
    std::unique_lock lock(user_mappings_mutex_);
    user_mappings_[mapping.server_id].push_back(mapping);
    return Result<void>();
}

Result<void> RemoteConnectionPoolRegistry::removeUserMapping(
    uint64_t server_id,
    const std::string& local_user)
{
    std::unique_lock lock(user_mappings_mutex_);

    auto it = user_mappings_.find(server_id);
    if (it == user_mappings_.end()) {
        return makeError(core::Status::NOT_FOUND, "No mappings for server");
    }

    auto& mappings = it->second;
    mappings.erase(
        std::remove_if(mappings.begin(), mappings.end(),
                       [&](const UserMapping& m) {
                           return m.local_user == local_user;
                       }),
        mappings.end());

    return Result<void>();
}

const UserMapping* RemoteConnectionPoolRegistry::getUserMapping(
    const std::string& server_name,
    const std::string& local_user) const
{
    const ServerDefinition* server = getServer(server_name);
    if (!server) {
        return nullptr;
    }
    return findUserMapping(server->server_id, local_user);
}

const UserMapping* RemoteConnectionPoolRegistry::findUserMapping(
    uint64_t server_id,
    const std::string& local_user) const
{
    std::shared_lock lock(user_mappings_mutex_);

    auto it = user_mappings_.find(server_id);
    if (it == user_mappings_.end()) {
        return nullptr;
    }

    for (const auto& mapping : it->second) {
        if (mapping.local_user == local_user) {
            return &mapping;
        }
    }

    return nullptr;
}

Result<void> RemoteConnectionPoolRegistry::warmupServer(
    const std::string& server_name,
    uint32_t count)
{
    // Warmup requires a user mapping - typically done per-user on first access
    return Result<void>();
}

Result<void> RemoteConnectionPoolRegistry::evictIdleConnections(
    const std::string& server_name)
{
    std::shared_lock lock(server_pools_mutex_);

    auto it = server_pools_.find(server_name);
    if (it == server_pools_.end()) {
        return makeError(core::Status::NOT_FOUND,
                         "Server not found: " + server_name);
    }

    return it->second->evictIdle();
}

Result<void> RemoteConnectionPoolRegistry::closeAllConnections(
    const std::string& server_name)
{
    std::shared_lock lock(server_pools_mutex_);

    auto it = server_pools_.find(server_name);
    if (it == server_pools_.end()) {
        return makeError(core::Status::NOT_FOUND,
                         "Server not found: " + server_name);
    }

    return it->second->shutdown();
}

bool RemoteConnectionPoolRegistry::isServerHealthy(const std::string& server_name) const {
    std::shared_lock lock(server_pools_mutex_);

    auto it = server_pools_.find(server_name);
    if (it == server_pools_.end()) {
        return false;
    }

    return it->second->isHealthy();
}

RegistryStats RemoteConnectionPoolRegistry::getStats() const {
    RegistryStats stats;

    std::shared_lock lock(server_pools_mutex_);
    stats.total_servers = static_cast<uint32_t>(server_pools_.size());

    for (const auto& [name, pool] : server_pools_) {
        auto server_stats = pool->getStats();
        if (server_stats.healthy) {
            stats.healthy_servers++;
        } else {
            stats.unhealthy_servers++;
        }
        stats.total_connections += server_stats.total_connections;
        stats.active_connections += server_stats.active_connections;
        stats.total_acquires += server_stats.total_acquires;
        stats.total_releases += server_stats.total_releases;
        stats.total_errors += server_stats.acquire_timeouts + server_stats.connection_errors;
    }

    return stats;
}

ServerPoolStats RemoteConnectionPoolRegistry::getServerStats(
    const std::string& server_name) const
{
    std::shared_lock lock(server_pools_mutex_);

    auto it = server_pools_.find(server_name);
    if (it == server_pools_.end()) {
        return ServerPoolStats{};
    }

    return it->second->getStats();
}

void RemoteConnectionPoolRegistry::startHealthChecker() {
    if (health_checker_) {
        health_checker_->start();
    }
}

void RemoteConnectionPoolRegistry::stopHealthChecker() {
    if (health_checker_) {
        health_checker_->stop();
    }
}

ServerPool* RemoteConnectionPoolRegistry::getServerPool(const std::string& server_name) {
    std::shared_lock lock(server_pools_mutex_);

    auto it = server_pools_.find(server_name);
    if (it == server_pools_.end()) {
        return nullptr;
    }

    return it->second.get();
}

}  // namespace fdw
}  // namespace scratchbird

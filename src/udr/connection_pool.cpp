/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

#include "scratchbird/udr/connection_pool.h"

#include <algorithm>

namespace scratchbird {
namespace udr {

// ============================================================================
// PooledConnection
// ============================================================================

PooledConnection::PooledConnection()
    : created_(std::chrono::steady_clock::now())
    , last_used_(created_) {
}

bool PooledConnection::isExpired(uint32_t max_lifetime_ms) const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - created_).count();
    return elapsed > static_cast<int64_t>(max_lifetime_ms);
}

bool PooledConnection::isIdleTimeout(uint32_t idle_timeout_ms) const {
    if (in_use_) return false;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_used_).count();
    return elapsed > static_cast<int64_t>(idle_timeout_ms);
}

// ============================================================================
// ConnectionPool
// ============================================================================

ConnectionPool::ConnectionPool(const std::string& name,
                               std::unique_ptr<ConnectionFactory> factory,
                               const ConnectionPoolConfig& config)
    : name_(name)
    , factory_(std::move(factory))
    , config_(config) {
}

ConnectionPool::~ConnectionPool() {
    shutdown();
}

core::Status ConnectionPool::initialize(core::ErrorContext* ctx) {
    if (running_) {
        return core::Status::OK;
    }
    
    running_ = true;
    
    // Create initial connections
    auto status = ensureMinimumConnections(ctx);
    if (status != core::Status::OK) {
        running_ = false;
        return status;
    }
    
    // Start maintenance thread
    startMaintenanceThread();
    
    return core::Status::OK;
}

void ConnectionPool::shutdown() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    stop_maintenance_ = true;
    available_cv_.notify_all();
    
    if (maintenance_thread_.joinable()) {
        maintenance_thread_.join();
    }
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    // Close all connections
    for (auto& conn : all_connections_) {
        if (conn) {
            factory_->destroyConnection(conn.get());
        }
    }
    
    all_connections_.clear();
    while (!idle_connections_.empty()) {
        idle_connections_.pop();
    }
    
    stats_.total_connections = 0;
    stats_.active_connections = 0;
    stats_.idle_connections = 0;
}

std::unique_ptr<PooledConnection> ConnectionPool::acquire(core::ErrorContext* ctx) {
    return acquireWithTimeout(config_.acquire_timeout_ms, ctx);
}

std::unique_ptr<PooledConnection> ConnectionPool::acquireWithTimeout(
    uint32_t timeout_ms,
    core::ErrorContext* ctx) {
    
    if (!running_) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Pool is not running",
                     __FILE__, __LINE__, __func__);
        }
        return nullptr;
    }
    
    stats_.total_requests++;
    
    auto start_time = std::chrono::steady_clock::now();
    
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    // Try to get an available connection
    while (true) {
        // Check for available idle connections
        while (!idle_connections_.empty()) {
            PooledConnection* conn = idle_connections_.front();
            idle_connections_.pop();
            stats_.idle_connections--;
            
            // Validate if needed
            if (config_.test_on_borrow) {
                lock.unlock();
                bool valid = validateConnection(conn, ctx);
                lock.lock();
                
                if (!valid) {
                    // Remove invalid connection
                    auto it = std::find_if(all_connections_.begin(),
                                          all_connections_.end(),
                                          [conn](const auto& c) {
                                              return c.get() == conn;
                                          });
                    if (it != all_connections_.end()) {
                        factory_->destroyConnection(it->get());
                        all_connections_.erase(it);
                        stats_.total_connections--;
                        stats_.validation_failures++;
                    }
                    continue;
                }
            }
            
            // Got a valid connection
            conn->markInUse();
            stats_.active_connections++;
            stats_.hits++;
            
            // Find and extract the connection from all_connections_
            auto it = std::find_if(all_connections_.begin(),
                                  all_connections_.end(),
                                  [conn](const auto& c) {
                                      return c.get() == conn;
                                  });
            if (it != all_connections_.end()) {
                auto result = std::move(*it);
                all_connections_.erase(it);
                
                // Track wait time
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time).count();
                stats_.wait_time_ms += elapsed;
                
                return result;
            }
        }
        
        // Try to create a new connection if under max
        if (stats_.total_connections < config_.max_size) {
            lock.unlock();
            auto conn = createConnection(ctx);
            lock.lock();
            
            if (conn) {
                conn->markInUse();
                stats_.active_connections++;
                stats_.total_connections++;
                stats_.misses++;
                
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time).count();
                stats_.wait_time_ms += elapsed;
                
                return conn;
            }
        }
        
        // No connections available - wait or fail
        if (!config_.block_when_exhausted) {
            stats_.misses++;
            if (ctx) {
                ctx->set(core::Status::CONFIGURATION_LIMIT_EXCEEDED,
                        "Connection pool exhausted",
                        __FILE__, __LINE__, __func__);
            }
            return nullptr;
        }
        
        // Wait for a connection to become available
        incrementWaiting();
        
        auto wait_result = available_cv_.wait_for(lock,
            std::chrono::milliseconds(timeout_ms),
            [this] { return !idle_connections_.empty() || !running_; });
        
        decrementWaiting();
        
        if (!running_) {
            if (ctx) {
                ctx->set(core::Status::INVALID_ARGUMENT, "Pool is shutting down",
                        __FILE__, __LINE__, __func__);
            }
            return nullptr;
        }
        
        if (!wait_result) {
            // Timeout
            stats_.misses++;
            if (ctx) {
                ctx->set(core::Status::LOCK_TIMEOUT,
                        "Timeout waiting for connection",
                        __FILE__, __LINE__, __func__);
            }
            return nullptr;
        }
        // Loop and try again
    }
}

void ConnectionPool::release(std::unique_ptr<PooledConnection> conn) {
    if (!conn) {
        return;
    }
    
    stats_.active_connections--;
    
    if (!running_) {
        factory_->destroyConnection(conn.get());
        return;
    }
    
    if (!conn->isOpen() || conn->isExpired(config_.max_lifetime_ms)) {
        factory_->destroyConnection(conn.get());
        stats_.total_connections--;
        return;
    }
    
    returnToPool(std::move(conn));
}

void ConnectionPool::returnToPool(std::unique_ptr<PooledConnection> conn) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    conn->markIdle();
    
    // Test on return if configured
    if (config_.test_on_return) {
        if (!validateConnection(conn.get(), nullptr)) {
            factory_->destroyConnection(conn.get());
            stats_.total_connections--;
            return;
        }
    }
    
    idle_connections_.push(conn.get());
    stats_.idle_connections++;
    all_connections_.push_back(std::move(conn));
    
    available_cv_.notify_one();
}

void ConnectionPool::invalidateAll() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    // Clear idle queue (connections still in all_connections_)
    while (!idle_connections_.empty()) {
        idle_connections_.pop();
    }
    stats_.idle_connections = 0;
    
    // Destroy all connections
    for (auto& conn : all_connections_) {
        if (conn) {
            factory_->destroyConnection(conn.get());
        }
    }
    all_connections_.clear();
    
    stats_.total_connections = 0;
    stats_.active_connections = 0;
}

void ConnectionPool::evictIdle() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    std::queue<PooledConnection*> new_idle;
    
    while (!idle_connections_.empty()) {
        PooledConnection* conn = idle_connections_.front();
        idle_connections_.pop();
        
        if (conn->isIdleTimeout(config_.idle_timeout_ms) ||
            conn->isExpired(config_.max_lifetime_ms)) {
            // Remove from all_connections_
            auto it = std::find_if(all_connections_.begin(),
                                  all_connections_.end(),
                                  [conn](const auto& c) {
                                      return c.get() == conn;
                                  });
            if (it != all_connections_.end()) {
                factory_->destroyConnection(it->get());
                all_connections_.erase(it);
                stats_.total_connections--;
                stats_.destroyed++;
            }
        } else {
            new_idle.push(conn);
        }
    }
    
    idle_connections_ = std::move(new_idle);
    stats_.idle_connections = static_cast<uint32_t>(idle_connections_.size());
}

core::Status ConnectionPool::ensureMinimumConnections(core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    uint32_t to_create = config_.min_size - stats_.total_connections;
    
    for (uint32_t i = 0; i < to_create; ++i) {
        auto conn = createConnection(ctx);
        if (!conn) {
            return core::Status::CONNECTION_FAILURE;
        }
        
        conn->markIdle();
        idle_connections_.push(conn.get());
        all_connections_.push_back(std::move(conn));
        
        stats_.total_connections++;
        stats_.idle_connections++;
        stats_.created++;
    }
    
    return core::Status::OK;
}

std::unique_ptr<PooledConnection> ConnectionPool::createConnection(
    core::ErrorContext* ctx) {
    
    for (uint32_t attempt = 0; attempt <= config_.max_retries; ++attempt) {
        auto conn = factory_->createConnection(ctx);
        if (conn) {
            conn->setPoolId(next_id_++);
            stats_.created++;
            return conn;
        }
        
        if (attempt < config_.max_retries) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(config_.retry_delay_ms));
        }
    }
    
    return nullptr;
}

bool ConnectionPool::validateConnection(PooledConnection* conn,
                                       core::ErrorContext* ctx) {
    if (!conn) return false;
    
    stats_.validated++;
    
    if (!conn->isOpen()) {
        return false;
    }
    
    if (!config_.validation_query.empty()) {
        // Would execute validation query here
        // For now, just use ping
        return conn->ping(ctx) == core::Status::OK;
    }
    
    return conn->ping(ctx) == core::Status::OK;
}

void ConnectionPool::startMaintenanceThread() {
    stop_maintenance_ = false;
    maintenance_thread_ = std::thread(&ConnectionPool::maintenanceLoop, this);
}

void ConnectionPool::stopMaintenanceThread() {
    stop_maintenance_ = true;
    if (maintenance_thread_.joinable()) {
        maintenance_thread_.join();
    }
}

void ConnectionPool::maintenanceLoop() {
    while (!stop_maintenance_) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config_.health_check_interval_ms));
        
        if (!running_) continue;
        
        // Evict idle connections
        evictIdle();
        
        // Ensure minimum connections
        if (stats_.total_connections < config_.min_size) {
            ensureMinimumConnections(nullptr);
        }
    }
}

// ============================================================================
// ScopedConnection
// ============================================================================

ScopedConnection::ScopedConnection(ConnectionPool& pool,
                                   std::unique_ptr<PooledConnection> conn)
    : pool_(pool)
    , conn_(std::move(conn)) {
}

ScopedConnection::~ScopedConnection() {
    if (conn_) {
        pool_.release(std::move(conn_));
    }
}

ScopedConnection::ScopedConnection(ScopedConnection&& other) noexcept
    : pool_(other.pool_)
    , conn_(std::move(other.conn_)) {
}

ScopedConnection& ScopedConnection::operator=(ScopedConnection&& other) noexcept {
    if (this != &other) {
        if (conn_) {
            pool_.release(std::move(conn_));
        }
        conn_ = std::move(other.conn_);
    }
    return *this;
}

std::unique_ptr<PooledConnection> ScopedConnection::release() {
    return std::move(conn_);
}

// ============================================================================
// ConnectionPoolManager
// ============================================================================

ConnectionPoolManager& ConnectionPoolManager::instance() {
    static ConnectionPoolManager instance;
    return instance;
}

void ConnectionPoolManager::registerPool(const std::string& name,
                                         std::unique_ptr<ConnectionPool> pool) {
    std::lock_guard<std::mutex> lock(mutex_);
    pools_[name] = std::move(pool);
}

void ConnectionPoolManager::unregisterPool(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pools_.find(name);
    if (it != pools_.end()) {
        it->second->shutdown();
        pools_.erase(it);
    }
}

ConnectionPool* ConnectionPoolManager::getPool(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = pools_.find(name);
    if (it != pools_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<std::string> ConnectionPoolManager::getPoolNames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (const auto& pair : pools_) {
        names.push_back(pair.first);
    }
    return names;
}

void ConnectionPoolManager::shutdownAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : pools_) {
        pair.second->shutdown();
    }
    pools_.clear();
}

std::vector<std::pair<std::string, ConnectionPoolStats>> 
ConnectionPoolManager::getAllStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<std::string, ConnectionPoolStats>> result;
    for (const auto& pair : pools_) {
        result.push_back({pair.first, pair.second->getStats()});
    }
    return result;
}

} // namespace udr
} // namespace scratchbird

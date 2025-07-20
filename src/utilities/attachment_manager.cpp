#include "attachment_manager.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <random>
#include <cstring>

// Constructor
AttachmentManager::AttachmentManager() 
    : maintenance_running(false) {
    // Initialize performance counters
    performance_counters["connections_created"] = 0;
    performance_counters["connections_destroyed"] = 0;
    performance_counters["connections_borrowed"] = 0;
    performance_counters["connections_returned"] = 0;
    performance_counters["connections_validated"] = 0;
    performance_counters["validation_failures"] = 0;
    performance_counters["connection_timeouts"] = 0;
    performance_counters["connection_errors"] = 0;
}

// Destructor
AttachmentManager::~AttachmentManager() {
    shutdown();
}

// Configuration
bool AttachmentManager::initialize(const SBEnhanced::ConnectionPoolConfig& config) {
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    try {
        this->config = config;
        
        // Initialize connection pool
        connection_pool.reserve(config.max_connections);
        
        // Create initial connections
        for (uint32_t i = 0; i < config.initial_connections; ++i) {
            auto connection = std::make_unique<SBEnhanced::ConnectionInfo>();
            connection->created_time = std::chrono::steady_clock::now();
            connection->last_used_time = connection->created_time;
            connection->last_validated_time = connection->created_time;
            connection->is_valid = true;
            connection->is_active = false;
            connection->in_use = false;
            
            connection_pool.push_back(std::move(connection));
            available_connections.push(i);
        }
        
        stats.total_connections = config.initial_connections;
        stats.idle_connections = config.initial_connections;
        
        // Start maintenance thread
        if (!startMaintenanceThread()) {
            logError("initialize", "Failed to start maintenance thread");
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("initialize", std::string("Exception: ") + e.what());
        return false;
    }
}

bool AttachmentManager::setDefaultConnection(const std::string& database, const std::string& username,
                                           const std::string& password, const std::string& role) {
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    default_database = database;
    default_username = username;
    default_password = password;
    default_role = role;
    
    return true;
}

bool AttachmentManager::shutdown() {
    try {
        // Stop maintenance thread
        stopMaintenanceThread();
        
        // Close all connections
        clearPool();
        
        // Close all database links
        std::lock_guard<std::mutex> links_lock(links_mutex);
        database_links.clear();
        
        return true;
        
    } catch (const std::exception& e) {
        logError("shutdown", std::string("Exception: ") + e.what());
        return false;
    }
}

// Connection management
bool AttachmentManager::createConnection(const std::string& database, const std::string& username,
                                       const std::string& password, const std::string& role,
                                       const std::map<std::string, std::string>& params) {
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        if (connection_pool.size() >= config.max_connections) {
            logError("createConnection", "Maximum connections reached");
            return false;
        }
        
        auto connection = std::make_unique<SBEnhanced::ConnectionInfo>();
        connection->database_path = database;
        connection->username = username;
        connection->password = password;
        connection->role = role;
        connection->connection_params = params;
        connection->created_time = std::chrono::steady_clock::now();
        connection->last_used_time = connection->created_time;
        connection->last_validated_time = connection->created_time;
        connection->is_valid = true;
        connection->is_active = false;
        connection->in_use = false;
        
        if (createConnectionInternal(database, username, password, role, params, connection.get())) {
            size_t index = connection_pool.size();
            connection_pool.push_back(std::move(connection));
            available_connections.push(index);
            
            stats.total_connections++;
            stats.idle_connections++;
            stats.connections_created++;
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            logPerformance("createConnection", duration);
            
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logError("createConnection", std::string("Exception: ") + e.what());
        return false;
    }
}

std::shared_ptr<SBEnhanced::ConnectionInfo> AttachmentManager::borrowConnection(std::chrono::seconds timeout) {
    std::unique_lock<std::mutex> lock(pool_mutex);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Wait for available connection
        if (!pool_condition.wait_for(lock, timeout, [this] { return !available_connections.empty(); })) {
            stats.connection_timeouts++;
            logError("borrowConnection", "Connection timeout");
            return nullptr;
        }
        
        // Get available connection
        size_t index = available_connections.front();
        available_connections.pop();
        
        auto& connection = connection_pool[index];
        
        // Validate connection if needed
        if (config.test_on_borrow && !validateConnectionInternal(connection.get())) {
            // Connection is invalid, try to recreate it
            if (!createConnectionInternal(connection->database_path, connection->username, 
                                        connection->password, connection->role, 
                                        connection->connection_params, connection.get())) {
                // Failed to recreate, put it back and return null
                available_connections.push(index);
                stats.validation_failures++;
                logError("borrowConnection", "Failed to validate/recreate connection");
                return nullptr;
            }
        }
        
        // Mark connection as in use
        connection->in_use = true;
        connection->is_active = true;
        connection->last_used_time = std::chrono::steady_clock::now();
        connection->owner_thread = std::this_thread::get_id();
        
        stats.active_connections++;
        stats.idle_connections--;
        stats.connections_borrowed++;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        stats.average_borrow_time = duration;
        logPerformance("borrowConnection", duration);
        
        return std::shared_ptr<SBEnhanced::ConnectionInfo>(connection.get(), [this](SBEnhanced::ConnectionInfo* conn) {
            this->returnConnection(std::shared_ptr<SBEnhanced::ConnectionInfo>(conn, [](SBEnhanced::ConnectionInfo*) {}));
        });
        
    } catch (const std::exception& e) {
        logError("borrowConnection", std::string("Exception: ") + e.what());
        return nullptr;
    }
}

bool AttachmentManager::returnConnection(std::shared_ptr<SBEnhanced::ConnectionInfo> connection) {
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    try {
        if (!connection) {
            return false;
        }
        
        // Find connection in pool
        auto it = std::find_if(connection_pool.begin(), connection_pool.end(),
                              [&connection](const std::unique_ptr<SBEnhanced::ConnectionInfo>& conn) {
                                  return conn.get() == connection.get();
                              });
        
        if (it == connection_pool.end()) {
            logError("returnConnection", "Connection not found in pool");
            return false;
        }
        
        size_t index = std::distance(connection_pool.begin(), it);
        
        // Validate connection if needed
        if (config.test_on_return && !validateConnectionInternal(connection.get())) {
            // Connection is invalid, mark it for removal
            connection->is_valid = false;
            stats.validation_failures++;
        }
        
        // Mark connection as not in use
        connection->in_use = false;
        connection->is_active = false;
        connection->last_used_time = std::chrono::steady_clock::now();
        connection->owner_thread = std::thread::id{};
        
        // Return to available pool if valid
        if (connection->is_valid) {
            available_connections.push(index);
            stats.idle_connections++;
        }
        
        stats.active_connections--;
        stats.connections_returned++;
        
        // Notify waiting threads
        pool_condition.notify_one();
        
        return true;
        
    } catch (const std::exception& e) {
        logError("returnConnection", std::string("Exception: ") + e.what());
        return false;
    }
}

bool AttachmentManager::testConnection(SBEnhanced::ConnectionInfo* connection) {
    if (!connection) {
        return false;
    }
    
    return validateConnectionInternal(connection);
}

bool AttachmentManager::validateConnection(SBEnhanced::ConnectionInfo* connection) {
    if (!connection) {
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    bool result = validateConnectionInternal(connection);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    stats.average_validation_time = duration;
    
    if (result) {
        stats.connections_validated++;
    } else {
        stats.validation_failures++;
    }
    
    return result;
}

// Connection pool operations
bool AttachmentManager::expandPool(uint32_t additional_connections) {
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    try {
        uint32_t new_total = connection_pool.size() + additional_connections;
        if (new_total > config.max_connections) {
            additional_connections = config.max_connections - connection_pool.size();
        }
        
        for (uint32_t i = 0; i < additional_connections; ++i) {
            if (!createConnection(default_database, default_username, default_password, default_role)) {
                logError("expandPool", "Failed to create connection " + std::to_string(i));
                return false;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("expandPool", std::string("Exception: ") + e.what());
        return false;
    }
}

bool AttachmentManager::shrinkPool(uint32_t remove_connections) {
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    try {
        uint32_t removed = 0;
        
        while (removed < remove_connections && !available_connections.empty()) {
            size_t index = available_connections.front();
            available_connections.pop();
            
            auto& connection = connection_pool[index];
            if (connection && !connection->in_use) {
                destroyConnectionInternal(connection.get());
                connection.reset();
                removed++;
                stats.total_connections--;
                stats.idle_connections--;
                stats.connections_destroyed++;
            }
        }
        
        // Remove null connections from pool
        connection_pool.erase(
            std::remove_if(connection_pool.begin(), connection_pool.end(),
                          [](const std::unique_ptr<SBEnhanced::ConnectionInfo>& conn) {
                              return conn == nullptr;
                          }),
            connection_pool.end());
        
        return true;
        
    } catch (const std::exception& e) {
        logError("shrinkPool", std::string("Exception: ") + e.what());
        return false;
    }
}

bool AttachmentManager::clearPool() {
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    try {
        // Close all connections
        for (auto& connection : connection_pool) {
            if (connection) {
                destroyConnectionInternal(connection.get());
            }
        }
        
        connection_pool.clear();
        while (!available_connections.empty()) {
            available_connections.pop();
        }
        
        stats.total_connections = 0;
        stats.active_connections = 0;
        stats.idle_connections = 0;
        
        return true;
        
    } catch (const std::exception& e) {
        logError("clearPool", std::string("Exception: ") + e.what());
        return false;
    }
}

bool AttachmentManager::warmupPool() {
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    try {
        // Create connections up to initial size
        while (connection_pool.size() < config.initial_connections) {
            if (!createConnection(default_database, default_username, default_password, default_role)) {
                break;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("warmupPool", std::string("Exception: ") + e.what());
        return false;
    }
}

// Statistics
SBEnhanced::ConnectionPoolStats AttachmentManager::getPoolStatistics() const {
    return stats;
}

std::vector<SBEnhanced::ConnectionInfo> AttachmentManager::getActiveConnections() const {
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    std::vector<SBEnhanced::ConnectionInfo> active_connections;
    for (const auto& connection : connection_pool) {
        if (connection && connection->is_active) {
            active_connections.push_back(*connection);
        }
    }
    
    return active_connections;
}

// Performance monitoring
bool AttachmentManager::enablePerformanceMonitoring(bool enable) {
    performance_monitoring_enabled = enable;
    return true;
}

std::map<std::string, uint64_t> AttachmentManager::getPerformanceCounters() const {
    std::lock_guard<std::mutex> lock(performance_mutex);
    
    std::map<std::string, uint64_t> counters;
    for (const auto& [key, value] : performance_counters) {
        counters[key] = value.load();
    }
    
    return counters;
}

bool AttachmentManager::resetPerformanceCounters() {
    std::lock_guard<std::mutex> lock(performance_mutex);
    
    for (auto& [key, value] : performance_counters) {
        value = 0;
    }
    
    return true;
}

// Error handling
std::vector<std::string> AttachmentManager::getErrorLog() const {
    std::lock_guard<std::mutex> lock(error_mutex);
    return error_log;
}

void AttachmentManager::clearErrorLog() {
    std::lock_guard<std::mutex> lock(error_mutex);
    error_log.clear();
}

uint64_t AttachmentManager::getErrorCount() const {
    return error_count.load();
}

std::string AttachmentManager::getLastError() const {
    std::lock_guard<std::mutex> lock(error_mutex);
    return error_log.empty() ? "" : error_log.back();
}

// Maintenance operations
bool AttachmentManager::startMaintenanceThread() {
    if (maintenance_running.load()) {
        return true;
    }
    
    maintenance_running = true;
    maintenance_thread = std::thread(&AttachmentManager::maintenanceLoop, this);
    
    return true;
}

bool AttachmentManager::stopMaintenanceThread() {
    if (!maintenance_running.load()) {
        return true;
    }
    
    maintenance_running = false;
    maintenance_condition.notify_all();
    
    if (maintenance_thread.joinable()) {
        maintenance_thread.join();
    }
    
    return true;
}

bool AttachmentManager::runMaintenance() {
    try {
        cleanupIdleConnections();
        validatePoolConnections();
        ensureMinimumConnections();
        return true;
    } catch (const std::exception& e) {
        logError("runMaintenance", std::string("Exception: ") + e.what());
        return false;
    }
}

// Configuration queries
SBEnhanced::ConnectionPoolConfig AttachmentManager::getConfiguration() const {
    return config;
}

bool AttachmentManager::updateConfiguration(const SBEnhanced::ConnectionPoolConfig& config) {
    std::lock_guard<std::mutex> lock(pool_mutex);
    this->config = config;
    return true;
}

// Utility methods
bool AttachmentManager::isInitialized() const {
    return !connection_pool.empty();
}

uint32_t AttachmentManager::getActiveConnectionCount() const {
    return stats.active_connections.load();
}

uint32_t AttachmentManager::getIdleConnectionCount() const {
    return stats.idle_connections.load();
}

uint32_t AttachmentManager::getTotalConnectionCount() const {
    return stats.total_connections.load();
}

// Private methods
bool AttachmentManager::createConnectionInternal(const std::string& database, const std::string& username,
                                               const std::string& password, const std::string& role,
                                               const std::map<std::string, std::string>& params,
                                               SBEnhanced::ConnectionInfo* connection) {
    try {
        // Build database parameter block
        std::string dpb = buildDPB(username, password, role, params);
        
        // Attempt to attach to database
        ISC_STATUS status_vector[20];
        if (isc_attach_database(status_vector, 
                               static_cast<short>(database.length()), 
                               database.c_str(),
                               &connection->db_handle,
                               static_cast<short>(dpb.length()),
                               dpb.c_str())) {
            logError("createConnectionInternal", formatISCError(status_vector));
            return false;
        }
        
        connection->is_valid = true;
        connection->is_active = false;
        connection->database_path = database;
        connection->username = username;
        connection->password = password;
        connection->role = role;
        connection->connection_params = params;
        
        return true;
        
    } catch (const std::exception& e) {
        logError("createConnectionInternal", std::string("Exception: ") + e.what());
        return false;
    }
}

bool AttachmentManager::destroyConnectionInternal(SBEnhanced::ConnectionInfo* connection) {
    try {
        if (!connection || connection->db_handle == 0) {
            return true;
        }
        
        ISC_STATUS status_vector[20];
        if (isc_detach_database(status_vector, &connection->db_handle)) {
            logError("destroyConnectionInternal", formatISCError(status_vector));
            return false;
        }
        
        connection->db_handle = 0;
        connection->is_valid = false;
        connection->is_active = false;
        
        return true;
        
    } catch (const std::exception& e) {
        logError("destroyConnectionInternal", std::string("Exception: ") + e.what());
        return false;
    }
}

bool AttachmentManager::validateConnectionInternal(SBEnhanced::ConnectionInfo* connection) {
    try {
        if (!connection || connection->db_handle == 0) {
            return false;
        }
        
        // Execute validation query
        ISC_STATUS status_vector[20];
        isc_tr_handle transaction = 0;
        isc_stmt_handle statement = 0;
        
        // Start transaction
        if (isc_start_transaction(status_vector, &transaction, 1, &connection->db_handle, 0, nullptr)) {
            logError("validateConnectionInternal", formatISCError(status_vector));
            return false;
        }
        
        // Allocate statement
        if (isc_dsql_allocate_statement(status_vector, &connection->db_handle, &statement)) {
            isc_rollback_transaction(status_vector, &transaction);
            logError("validateConnectionInternal", formatISCError(status_vector));
            return false;
        }
        
        // Prepare validation query
        std::string validation_query = config.validation_query;
        if (isc_dsql_prepare(status_vector, &transaction, &statement, 0, validation_query.c_str(), SQL_DIALECT_V6, nullptr)) {
            isc_dsql_free_statement(status_vector, &statement, DSQL_drop);
            isc_rollback_transaction(status_vector, &transaction);
            logError("validateConnectionInternal", formatISCError(status_vector));
            return false;
        }
        
        // Execute query
        if (isc_dsql_execute(status_vector, &transaction, &statement, SQL_DIALECT_V6, nullptr)) {
            isc_dsql_free_statement(status_vector, &statement, DSQL_drop);
            isc_rollback_transaction(status_vector, &transaction);
            logError("validateConnectionInternal", formatISCError(status_vector));
            return false;
        }
        
        // Cleanup
        isc_dsql_free_statement(status_vector, &statement, DSQL_drop);
        isc_commit_transaction(status_vector, &transaction);
        
        connection->last_validated_time = std::chrono::steady_clock::now();
        connection->is_valid = true;
        
        return true;
        
    } catch (const std::exception& e) {
        logError("validateConnectionInternal", std::string("Exception: ") + e.what());
        return false;
    }
}

// Connection pool maintenance
void AttachmentManager::maintenanceLoop() {
    while (maintenance_running.load()) {
        try {
            std::unique_lock<std::mutex> lock(maintenance_mutex);
            
            // Wait for cleanup interval
            if (maintenance_condition.wait_for(lock, config.cleanup_interval, [this] { return !maintenance_running.load(); })) {
                break; // Shutdown requested
            }
            
            // Run maintenance tasks
            runMaintenance();
            
        } catch (const std::exception& e) {
            logError("maintenanceLoop", std::string("Exception: ") + e.what());
        }
    }
}

bool AttachmentManager::cleanupIdleConnections() {
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    try {
        auto now = std::chrono::steady_clock::now();
        uint32_t cleaned = 0;
        
        for (auto it = connection_pool.begin(); it != connection_pool.end(); ) {
            auto& connection = *it;
            
            if (connection && !connection->in_use && isConnectionIdle(connection.get())) {
                destroyConnectionInternal(connection.get());
                it = connection_pool.erase(it);
                cleaned++;
                stats.total_connections--;
                stats.idle_connections--;
                stats.connections_destroyed++;
            } else {
                ++it;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("cleanupIdleConnections", std::string("Exception: ") + e.what());
        return false;
    }
}

bool AttachmentManager::validatePoolConnections() {
    if (!config.test_while_idle) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    try {
        for (auto& connection : connection_pool) {
            if (connection && !connection->in_use) {
                if (!validateConnectionInternal(connection.get())) {
                    connection->is_valid = false;
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("validatePoolConnections", std::string("Exception: ") + e.what());
        return false;
    }
}

bool AttachmentManager::ensureMinimumConnections() {
    std::lock_guard<std::mutex> lock(pool_mutex);
    
    try {
        while (connection_pool.size() < config.min_connections) {
            if (!createConnection(default_database, default_username, default_password, default_role)) {
                break;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("ensureMinimumConnections", std::string("Exception: ") + e.what());
        return false;
    }
}

// Database parameter building
std::string AttachmentManager::buildDPB(const std::string& username, const std::string& password,
                                       const std::string& role, const std::map<std::string, std::string>& params) {
    std::string dpb;
    dpb.push_back(isc_dpb_version1);
    
    // Add username
    if (!username.empty()) {
        dpb.push_back(isc_dpb_user_name);
        dpb.push_back(static_cast<char>(username.length()));
        dpb.append(username);
    }
    
    // Add password
    if (!password.empty()) {
        dpb.push_back(isc_dpb_password);
        dpb.push_back(static_cast<char>(password.length()));
        dpb.append(password);
    }
    
    // Add role
    if (!role.empty()) {
        dpb.push_back(isc_dpb_sql_role_name);
        dpb.push_back(static_cast<char>(role.length()));
        dpb.append(role);
    }
    
    // Add additional parameters
    for (const auto& [key, value] : params) {
        if (key == "charset") {
            dpb.push_back(isc_dpb_lc_ctype);
            dpb.push_back(static_cast<char>(value.length()));
            dpb.append(value);
        } else if (key == "dialect") {
            dpb.push_back(isc_dpb_sql_dialect);
            dpb.push_back(4);
            int dialect = std::stoi(value);
            dpb.append(reinterpret_cast<const char*>(&dialect), 4);
        }
        // Add more parameter types as needed
    }
    
    return dpb;
}

// Error handling helpers
void AttachmentManager::logError(const std::string& operation, const std::string& error) const {
    std::lock_guard<std::mutex> lock(error_mutex);
    
    std::string full_error = "[AttachmentManager] " + operation + ": " + error;
    error_log.push_back(full_error);
    
    // Keep error log size manageable
    if (error_log.size() > 1000) {
        error_log.erase(error_log.begin());
    }
    
    error_count++;
    std::cerr << full_error << std::endl;
}

void AttachmentManager::logPerformance(const std::string& operation, std::chrono::microseconds duration) const {
    if (performance_monitoring_enabled.load()) {
        std::cout << "[AttachmentManager] Performance: " << operation << " took " << duration.count() << " microseconds" << std::endl;
    }
}

std::string AttachmentManager::formatISCError(const ISC_STATUS* status_vector) const {
    std::string error_msg;
    char temp_buffer[512];
    
    for (int i = 0; status_vector[i] != isc_arg_end; i++) {
        if (status_vector[i] == isc_arg_gds) {
            isc_interprete(temp_buffer, &status_vector);
            error_msg += temp_buffer;
            error_msg += " ";
        }
    }
    
    return error_msg.empty() ? "Unknown ISC error" : error_msg;
}

// Utility helpers
bool AttachmentManager::isConnectionExpired(const SBEnhanced::ConnectionInfo* connection) const {
    if (!connection) {
        return true;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto age = now - connection->created_time;
    
    // For now, connections don't expire by age
    // This could be extended to support connection max lifetime
    return false;
}

bool AttachmentManager::isConnectionIdle(const SBEnhanced::ConnectionInfo* connection) const {
    if (!connection || connection->in_use) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto idle_time = now - connection->last_used_time;
    
    return idle_time > config.idle_timeout;
}

std::string AttachmentManager::generateConnectionId(const SBEnhanced::ConnectionInfo* connection) const {
    if (!connection) {
        return "null";
    }
    
    std::ostringstream oss;
    oss << connection->database_path << "_" << connection->username << "_" << connection->created_time.time_since_epoch().count();
    return oss.str();
}
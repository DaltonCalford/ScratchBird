#include "sb_database_enhanced.h"
#include "attachment_manager.h"
#include "transaction_manager.h"
#include "service_manager.h"
#include "metadata_cache.h"
#include "schema_cache.h"
#include "query_processor.h"
#include "result_set_manager.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>

// Constructor
SBDatabaseEnhanced::SBDatabaseEnhanced() 
    : SBDatabase(),
      cached_stats{},
      stats_cache_time{},
      stats_cache_duration{30} {
    // Initialize default configuration
    config_options["connection_timeout"] = "30";
    config_options["query_timeout"] = "300";
    config_options["cache_size"] = "100";
    config_options["enable_statistics"] = "true";
    config_options["enable_performance_monitoring"] = "true";
    config_options["enable_query_logging"] = "false";
    config_options["log_level"] = "info";
}

// Destructor
SBDatabaseEnhanced::~SBDatabaseEnhanced() {
    shutdownManagers();
}

// Enhanced connection management
bool SBDatabaseEnhanced::connectEnhanced(const std::string& db_name, const std::string& user, 
                                        const std::string& pass, const std::string& role, 
                                        bool trusted, const std::map<std::string, std::string>& options) {
    std::lock_guard<std::recursive_mutex> lock(connection_mutex);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Merge provided options with defaults
        auto merged_options = config_options;
        for (const auto& [key, value] : options) {
            merged_options[key] = value;
        }
        config_options = merged_options;
        
        // First establish basic connection
        if (!connect(db_name, user, pass, role, trusted)) {
            logError("connectEnhanced", "Failed to establish basic connection");
            return false;
        }
        
        // Initialize enhanced managers
        if (!initializeManagers()) {
            logError("connectEnhanced", "Failed to initialize enhanced managers");
            disconnect();
            return false;
        }
        
        // Set up connection in attachment manager
        if (attachment_mgr) {
            SBEnhanced::ConnectionPoolConfig pool_config;
            pool_config.min_connections = 1;
            pool_config.max_connections = std::stoi(config_options.at("connection_timeout"));
            pool_config.connection_timeout = std::chrono::seconds{std::stoi(config_options.at("connection_timeout"))};
            
            if (!attachment_mgr->initialize(pool_config)) {
                logError("connectEnhanced", "Failed to initialize attachment manager");
                return false;
            }
            
            if (!attachment_mgr->setDefaultConnection(db_name, user, pass, role)) {
                logError("connectEnhanced", "Failed to set default connection");
                return false;
            }
        }
        
        // Initialize transaction manager
        if (transaction_mgr) {
            SBEnhanced::TransactionConfig tx_config;
            tx_config.isolation_level = SBEnhanced::IsolationLevel::READ_COMMITTED;
            tx_config.access_mode = SBEnhanced::AccessMode::READ_WRITE;
            
            if (!transaction_mgr->initialize(tx_config)) {
                logError("connectEnhanced", "Failed to initialize transaction manager");
                return false;
            }
        }
        
        // Initialize service manager
        if (service_mgr) {
            std::string server_name = "localhost";
            if (auto pos = db_name.find(':'); pos != std::string::npos) {
                server_name = db_name.substr(0, pos);
            }
            
            if (!service_mgr->initialize(server_name, user, pass, 4)) {
                logError("connectEnhanced", "Failed to initialize service manager");
                return false;
            }
        }
        
        // Initialize metadata cache
        if (metadata_cache) {
            SBEnhanced::CacheConfig cache_config;
            cache_config.max_size_bytes = std::stoi(config_options.at("cache_size")) * 1024 * 1024;
            cache_config.enable_statistics = (config_options.at("enable_statistics") == "true");
            
            if (!metadata_cache->initialize(cache_config)) {
                logError("connectEnhanced", "Failed to initialize metadata cache");
                return false;
            }
        }
        
        // Initialize schema cache
        if (schema_cache) {
            // Schema cache initialization will be implemented when we create schema_cache.h
            // For now, we'll skip this
        }
        
        // Initialize query processor
        if (query_processor) {
            // Query processor initialization will be implemented when we create query_processor.h
            // For now, we'll skip this
        }
        
        // Initialize result set manager
        if (result_mgr) {
            // Result set manager initialization will be implemented when we create result_set_manager.h
            // For now, we'll skip this
        }
        
        // Enable performance monitoring if requested
        if (config_options.at("enable_performance_monitoring") == "true") {
            enablePerformanceMonitoring(true);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        logPerformance("connectEnhanced", duration);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("connectEnhanced", std::string("Exception: ") + e.what());
        return false;
    }
}

bool SBDatabaseEnhanced::disconnectEnhanced() {
    std::lock_guard<std::recursive_mutex> lock(connection_mutex);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Shutdown enhanced managers
        shutdownManagers();
        
        // Disconnect base connection
        bool result = disconnect();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        logPerformance("disconnectEnhanced", duration);
        
        return result;
        
    } catch (const std::exception& e) {
        logError("disconnectEnhanced", std::string("Exception: ") + e.what());
        return false;
    }
}

bool SBDatabaseEnhanced::isConnectedEnhanced() const {
    std::lock_guard<std::recursive_mutex> lock(connection_mutex);
    return isConnected() && attachment_mgr && attachment_mgr->isInitialized();
}

// Configuration
bool SBDatabaseEnhanced::setConfigOption(const std::string& key, const std::string& value) {
    std::lock_guard<std::recursive_mutex> lock(connection_mutex);
    config_options[key] = value;
    return true;
}

std::string SBDatabaseEnhanced::getConfigOption(const std::string& key) const {
    std::lock_guard<std::recursive_mutex> lock(connection_mutex);
    auto it = config_options.find(key);
    return (it != config_options.end()) ? it->second : "";
}

std::map<std::string, std::string> SBDatabaseEnhanced::getAllConfigOptions() const {
    std::lock_guard<std::recursive_mutex> lock(connection_mutex);
    return config_options;
}

// Enhanced metadata operations
bool SBDatabaseEnhanced::extractDDL(const std::string& object_name, SBEnhanced::DDLType type, std::string& ddl) {
    if (!isConnectedEnhanced()) {
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Check metadata cache first
        std::string cache_key = "ddl_" + object_name + "_" + std::to_string(static_cast<int>(type));
        if (metadata_cache && metadata_cache->get(cache_key, ddl)) {
            return true;
        }
        
        // Generate DDL based on type
        std::ostringstream ddl_stream;
        bool success = false;
        
        switch (type) {
            case SBEnhanced::DDLType::TABLE:
                success = extractTableDDL(object_name, ddl_stream);
                break;
            case SBEnhanced::DDLType::VIEW:
                success = extractViewDDL(object_name, ddl_stream);
                break;
            case SBEnhanced::DDLType::PROCEDURE:
                success = extractProcedureDDL(object_name, ddl_stream);
                break;
            case SBEnhanced::DDLType::FUNCTION:
                success = extractFunctionDDL(object_name, ddl_stream);
                break;
            case SBEnhanced::DDLType::TRIGGER:
                success = extractTriggerDDL(object_name, ddl_stream);
                break;
            case SBEnhanced::DDLType::DOMAIN:
                success = extractDomainDDL(object_name, ddl_stream);
                break;
            case SBEnhanced::DDLType::EXCEPTION:
                success = extractExceptionDDL(object_name, ddl_stream);
                break;
            case SBEnhanced::DDLType::GENERATOR:
                success = extractGeneratorDDL(object_name, ddl_stream);
                break;
            case SBEnhanced::DDLType::ROLE:
                success = extractRoleDDL(object_name, ddl_stream);
                break;
            case SBEnhanced::DDLType::INDEX:
                success = extractIndexDDL(object_name, ddl_stream);
                break;
            default:
                logError("extractDDL", "Unsupported DDL type");
                return false;
        }
        
        if (success) {
            ddl = ddl_stream.str();
            
            // Cache the result
            if (metadata_cache) {
                metadata_cache->put(cache_key, ddl, SBEnhanced::MetadataType::UNKNOWN);
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            logPerformance("extractDDL", duration);
            
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logError("extractDDL", std::string("Exception: ") + e.what());
        return false;
    }
}

bool SBDatabaseEnhanced::extractAllDDL(std::ostream& output, const std::vector<SBEnhanced::DDLType>& types) {
    if (!isConnectedEnhanced()) {
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        std::vector<SBEnhanced::DDLType> extract_types = types;
        if (extract_types.empty()) {
            // Default to all types
            extract_types = {
                SBEnhanced::DDLType::DOMAIN,
                SBEnhanced::DDLType::EXCEPTION,
                SBEnhanced::DDLType::GENERATOR,
                SBEnhanced::DDLType::ROLE,
                SBEnhanced::DDLType::TABLE,
                SBEnhanced::DDLType::VIEW,
                SBEnhanced::DDLType::INDEX,
                SBEnhanced::DDLType::PROCEDURE,
                SBEnhanced::DDLType::FUNCTION,
                SBEnhanced::DDLType::TRIGGER
            };
        }
        
        output << "/* Generated DDL for database */\n";
        output << "/* Generated at: " << std::chrono::system_clock::now().time_since_epoch().count() << " */\n\n";
        
        for (auto type : extract_types) {
            output << "/* " << ddlTypeToString(type) << " definitions */\n";
            
            std::vector<std::string> object_names;
            if (getObjectNamesByType(type, object_names)) {
                for (const auto& name : object_names) {
                    std::string ddl;
                    if (extractDDL(name, type, ddl)) {
                        output << ddl << "\n\n";
                    }
                }
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        logPerformance("extractAllDDL", duration);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("extractAllDDL", std::string("Exception: ") + e.what());
        return false;
    }
}

bool SBDatabaseEnhanced::getObjectDependencies(const std::string& object_name, std::vector<SBEnhanced::ObjectDependency>& deps) {
    if (!isConnectedEnhanced()) {
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Check metadata cache first
        std::string cache_key = "deps_" + object_name;
        std::string cached_deps;
        if (metadata_cache && metadata_cache->get(cache_key, cached_deps)) {
            return deserializeDependencies(cached_deps, deps);
        }
        
        // Query dependencies from system tables
        std::string sql = R"(
            SELECT DISTINCT 
                d.RDB$DEPENDENT_NAME,
                d.RDB$DEPENDENT_TYPE,
                d.RDB$DEPENDED_ON_NAME,
                d.RDB$DEPENDED_ON_TYPE,
                d.RDB$FIELD_NAME
            FROM RDB$DEPENDENCIES d
            WHERE d.RDB$DEPENDENT_NAME = ? OR d.RDB$DEPENDED_ON_NAME = ?
            ORDER BY d.RDB$DEPENDENT_NAME
        )";
        
        std::vector<std::vector<std::string>> results;
        std::vector<std::string> columns;
        
        // For now, we'll use a simplified approach without parameters
        std::string simple_sql = R"(
            SELECT DISTINCT 
                d.RDB$DEPENDENT_NAME,
                d.RDB$DEPENDENT_TYPE,
                d.RDB$DEPENDED_ON_NAME,
                d.RDB$DEPENDED_ON_TYPE,
                d.RDB$FIELD_NAME
            FROM RDB$DEPENDENCIES d
            WHERE d.RDB$DEPENDENT_NAME = ')" + object_name + R"(' OR d.RDB$DEPENDED_ON_NAME = ')" + object_name + R"('
            ORDER BY d.RDB$DEPENDENT_NAME
        )";
        
        if (executeSelect(simple_sql, results, columns)) {
            deps.clear();
            for (const auto& row : results) {
                if (row.size() >= 4) {
                    SBEnhanced::ObjectDependency dep;
                    dep.object_name = trim(row[2]);
                    dep.object_type = dependencyTypeToString(std::stoi(row[3]));
                    dep.dependent_name = trim(row[0]);
                    dep.dependent_type = dependencyTypeToString(std::stoi(row[1]));
                    dep.dependency_type = "DEPENDS_ON";
                    dep.dependency_level = 1;
                    deps.push_back(dep);
                }
            }
            
            // Cache the result
            if (metadata_cache) {
                std::string serialized_deps = serializeDependencies(deps);
                metadata_cache->put(cache_key, serialized_deps, SBEnhanced::MetadataType::UNKNOWN);
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            logPerformance("getObjectDependencies", duration);
            
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logError("getObjectDependencies", std::string("Exception: ") + e.what());
        return false;
    }
}

bool SBDatabaseEnhanced::getSystemStatistics(SBEnhanced::SystemStats& stats) {
    if (!isConnectedEnhanced()) {
        return false;
    }
    
    std::shared_lock<std::shared_mutex> lock(cache_mutex);
    
    // Check if cached stats are still valid
    if (isStatsCacheValid()) {
        stats = cached_stats;
        return true;
    }
    
    lock.unlock();
    std::unique_lock<std::shared_mutex> write_lock(cache_mutex);
    
    // Double-check after acquiring write lock
    if (isStatsCacheValid()) {
        stats = cached_stats;
        return true;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        if (updateSystemStats(cached_stats)) {
            stats_cache_time = std::chrono::steady_clock::now();
            stats = cached_stats;
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            logPerformance("getSystemStatistics", duration);
            
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logError("getSystemStatistics", std::string("Exception: ") + e.what());
        return false;
    }
}

bool SBDatabaseEnhanced::refreshStatistics() {
    std::unique_lock<std::shared_mutex> lock(cache_mutex);
    stats_cache_time = std::chrono::steady_clock::time_point{}; // Invalidate cache
    return true;
}

// Performance monitoring
bool SBDatabaseEnhanced::enablePerformanceMonitoring(bool enable) {
    if (attachment_mgr) {
        attachment_mgr->enablePerformanceMonitoring(enable);
    }
    if (transaction_mgr) {
        transaction_mgr->enablePerformanceMonitoring(enable);
    }
    if (metadata_cache) {
        // Metadata cache performance monitoring will be handled internally
    }
    return true;
}

std::map<std::string, uint64_t> SBDatabaseEnhanced::getPerformanceCounters() const {
    std::map<std::string, uint64_t> counters;
    
    if (attachment_mgr) {
        auto mgr_counters = attachment_mgr->getPerformanceCounters();
        for (const auto& [key, value] : mgr_counters) {
            counters["attachment_" + key] = value;
        }
    }
    
    if (transaction_mgr) {
        auto mgr_counters = transaction_mgr->getPerformanceCounters();
        for (const auto& [key, value] : mgr_counters) {
            counters["transaction_" + key] = value;
        }
    }
    
    return counters;
}

bool SBDatabaseEnhanced::resetPerformanceCounters() {
    bool success = true;
    
    if (attachment_mgr) {
        success &= attachment_mgr->resetPerformanceCounters();
    }
    if (transaction_mgr) {
        success &= transaction_mgr->resetPerformanceCounters();
    }
    
    return success;
}

// Utility methods
std::string SBDatabaseEnhanced::formatError(const std::string& context) const {
    std::string base_error = getLastError();
    if (context.empty()) {
        return base_error;
    }
    return context + ": " + base_error;
}

bool SBDatabaseEnhanced::testConnection() const {
    if (!isConnectedEnhanced()) {
        return false;
    }
    
    try {
        std::vector<std::vector<std::string>> results;
        std::vector<std::string> columns;
        return const_cast<SBDatabaseEnhanced*>(this)->executeSelect("SELECT 1 FROM RDB$DATABASE", results, columns);
    } catch (...) {
        return false;
    }
}

std::string SBDatabaseEnhanced::getServerVersion() const {
    if (!isConnectedEnhanced()) {
        return "Unknown";
    }
    
    try {
        std::vector<std::vector<std::string>> results;
        std::vector<std::string> columns;
        if (const_cast<SBDatabaseEnhanced*>(this)->executeSelect("SELECT RDB$GET_CONTEXT('SYSTEM', 'ENGINE_VERSION') FROM RDB$DATABASE", results, columns)) {
            if (!results.empty() && !results[0].empty()) {
                return trim(results[0][0]);
            }
        }
    } catch (...) {
        // Ignore errors
    }
    
    return "Unknown";
}

std::string SBDatabaseEnhanced::getClientVersion() const {
    return "ScratchBird Enhanced Client 0.5.0";
}

std::map<std::string, std::string> SBDatabaseEnhanced::getConnectionInfo() const {
    std::map<std::string, std::string> info;
    
    info["client_version"] = getClientVersion();
    info["server_version"] = getServerVersion();
    info["connected"] = isConnectedEnhanced() ? "true" : "false";
    
    if (attachment_mgr) {
        info["active_connections"] = std::to_string(attachment_mgr->getActiveConnectionCount());
        info["total_connections"] = std::to_string(attachment_mgr->getTotalConnectionCount());
    }
    
    if (transaction_mgr) {
        auto stats = transaction_mgr->getStatistics();
        info["active_transactions"] = std::to_string(stats.active_transactions.load());
        info["total_transactions"] = std::to_string(stats.total_transactions.load());
    }
    
    return info;
}

// Private methods
bool SBDatabaseEnhanced::initializeManagers() {
    try {
        // Initialize attachment manager
        attachment_mgr = std::make_unique<AttachmentManager>();
        
        // Initialize transaction manager
        transaction_mgr = std::make_unique<TransactionManager>();
        
        // Initialize service manager
        service_mgr = std::make_unique<ServiceManager>();
        
        // Initialize metadata cache
        metadata_cache = std::make_unique<MetadataCache>();
        
        // Initialize schema cache (will be implemented later)
        // schema_cache = std::make_unique<SchemaCache>();
        
        // Initialize query processor (will be implemented later)
        // query_processor = std::make_unique<QueryProcessor>();
        
        // Initialize result set manager (will be implemented later)
        // result_mgr = std::make_unique<ResultSetManager>();
        
        return true;
        
    } catch (const std::exception& e) {
        logError("initializeManagers", std::string("Exception: ") + e.what());
        return false;
    }
}

bool SBDatabaseEnhanced::shutdownManagers() {
    try {
        // Shutdown in reverse order
        if (result_mgr) {
            result_mgr.reset();
        }
        
        if (query_processor) {
            query_processor.reset();
        }
        
        if (schema_cache) {
            schema_cache.reset();
        }
        
        if (metadata_cache) {
            metadata_cache->shutdown();
            metadata_cache.reset();
        }
        
        if (service_mgr) {
            service_mgr->shutdown();
            service_mgr.reset();
        }
        
        if (transaction_mgr) {
            transaction_mgr->shutdown();
            transaction_mgr.reset();
        }
        
        if (attachment_mgr) {
            attachment_mgr->shutdown();
            attachment_mgr.reset();
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("shutdownManagers", std::string("Exception: ") + e.what());
        return false;
    }
}

bool SBDatabaseEnhanced::updateSystemStats(SBEnhanced::SystemStats& stats) const {
    try {
        // Get database statistics
        DatabaseStats db_stats;
        if (const_cast<SBDatabaseEnhanced*>(this)->getDatabaseStats(db_stats)) {
            stats.database_size = db_stats.page_count * db_stats.page_size;
            stats.page_size = db_stats.page_size;
            stats.page_count = db_stats.page_count;
            stats.allocated_pages = db_stats.allocated_pages;
            stats.free_pages = db_stats.free_pages;
            stats.database_version = db_stats.database_version;
            stats.creation_date = db_stats.creation_date;
            stats.force_writes = db_stats.force_writes;
            stats.read_only = db_stats.read_only;
        }
        
        // Get connection statistics
        if (attachment_mgr) {
            auto pool_stats = attachment_mgr->getPoolStatistics();
            stats.active_connections = pool_stats.active_connections.load();
            stats.total_connections = pool_stats.total_connections.load();
            stats.peak_connections = pool_stats.peak_connections.load();
        }
        
        // Get transaction statistics
        if (transaction_mgr) {
            auto tx_stats = transaction_mgr->getStatistics();
            stats.active_transactions = tx_stats.active_transactions.load();
            stats.committed_transactions = tx_stats.committed_transactions.load();
            stats.rolled_back_transactions = tx_stats.rolled_back_transactions.load();
        }
        
        // Get metadata counts
        std::vector<std::vector<std::string>> results;
        std::vector<std::string> columns;
        
        if (const_cast<SBDatabaseEnhanced*>(this)->executeSelect("SELECT COUNT(*) FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0", results, columns)) {
            if (!results.empty() && !results[0].empty()) {
                stats.table_count = std::stoul(results[0][0]);
            }
        }
        
        if (const_cast<SBDatabaseEnhanced*>(this)->executeSelect("SELECT COUNT(*) FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0 AND RDB$VIEW_BLR IS NOT NULL", results, columns)) {
            if (!results.empty() && !results[0].empty()) {
                stats.view_count = std::stoul(results[0][0]);
            }
        }
        
        if (const_cast<SBDatabaseEnhanced*>(this)->executeSelect("SELECT COUNT(*) FROM RDB$PROCEDURES WHERE RDB$SYSTEM_FLAG = 0", results, columns)) {
            if (!results.empty() && !results[0].empty()) {
                stats.procedure_count = std::stoul(results[0][0]);
            }
        }
        
        if (const_cast<SBDatabaseEnhanced*>(this)->executeSelect("SELECT COUNT(*) FROM RDB$FUNCTIONS WHERE RDB$SYSTEM_FLAG = 0", results, columns)) {
            if (!results.empty() && !results[0].empty()) {
                stats.function_count = std::stoul(results[0][0]);
            }
        }
        
        if (const_cast<SBDatabaseEnhanced*>(this)->executeSelect("SELECT COUNT(*) FROM RDB$TRIGGERS WHERE RDB$SYSTEM_FLAG = 0", results, columns)) {
            if (!results.empty() && !results[0].empty()) {
                stats.trigger_count = std::stoul(results[0][0]);
            }
        }
        
        if (const_cast<SBDatabaseEnhanced*>(this)->executeSelect("SELECT COUNT(*) FROM RDB$INDICES WHERE RDB$SYSTEM_FLAG = 0", results, columns)) {
            if (!results.empty() && !results[0].empty()) {
                stats.index_count = std::stoul(results[0][0]);
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("updateSystemStats", std::string("Exception: ") + e.what());
        return false;
    }
}

bool SBDatabaseEnhanced::isStatsCacheValid() const {
    auto now = std::chrono::steady_clock::now();
    return (now - stats_cache_time) < stats_cache_duration;
}

void SBDatabaseEnhanced::logError(const std::string& operation, const std::string& error) const {
    std::cerr << "[SBDatabaseEnhanced] Error in " << operation << ": " << error << std::endl;
}

void SBDatabaseEnhanced::logPerformance(const std::string& operation, std::chrono::microseconds duration) const {
    if (getConfigOption("enable_performance_monitoring") == "true") {
        std::cout << "[SBDatabaseEnhanced] Performance: " << operation << " took " << duration.count() << " microseconds" << std::endl;
    }
}

// Helper functions for DDL extraction
bool SBDatabaseEnhanced::extractTableDDL(const std::string& table_name, std::ostringstream& output) {
    // This is a simplified implementation
    // In a full implementation, this would generate complete CREATE TABLE statements
    output << "CREATE TABLE " << table_name << " (\n";
    output << "    /* Table definition would be generated here */\n";
    output << ");";
    return true;
}

bool SBDatabaseEnhanced::extractViewDDL(const std::string& view_name, std::ostringstream& output) {
    output << "CREATE VIEW " << view_name << " AS\n";
    output << "    /* View definition would be generated here */;";
    return true;
}

bool SBDatabaseEnhanced::extractProcedureDDL(const std::string& procedure_name, std::ostringstream& output) {
    output << "CREATE PROCEDURE " << procedure_name << "\n";
    output << "AS\n";
    output << "BEGIN\n";
    output << "    /* Procedure body would be generated here */\n";
    output << "END;";
    return true;
}

bool SBDatabaseEnhanced::extractFunctionDDL(const std::string& function_name, std::ostringstream& output) {
    output << "CREATE FUNCTION " << function_name << "\n";
    output << "RETURNS /* return type */\n";
    output << "AS\n";
    output << "BEGIN\n";
    output << "    /* Function body would be generated here */\n";
    output << "END;";
    return true;
}

bool SBDatabaseEnhanced::extractTriggerDDL(const std::string& trigger_name, std::ostringstream& output) {
    output << "CREATE TRIGGER " << trigger_name << "\n";
    output << "FOR /* table_name */\n";
    output << "/* BEFORE/AFTER */ /* INSERT/UPDATE/DELETE */\n";
    output << "AS\n";
    output << "BEGIN\n";
    output << "    /* Trigger body would be generated here */\n";
    output << "END;";
    return true;
}

bool SBDatabaseEnhanced::extractDomainDDL(const std::string& domain_name, std::ostringstream& output) {
    output << "CREATE DOMAIN " << domain_name << " AS /* data type */;";
    return true;
}

bool SBDatabaseEnhanced::extractExceptionDDL(const std::string& exception_name, std::ostringstream& output) {
    output << "CREATE EXCEPTION " << exception_name << " '/* exception message */';";
    return true;
}

bool SBDatabaseEnhanced::extractGeneratorDDL(const std::string& generator_name, std::ostringstream& output) {
    output << "CREATE GENERATOR " << generator_name << ";";
    return true;
}

bool SBDatabaseEnhanced::extractRoleDDL(const std::string& role_name, std::ostringstream& output) {
    output << "CREATE ROLE " << role_name << ";";
    return true;
}

bool SBDatabaseEnhanced::extractIndexDDL(const std::string& index_name, std::ostringstream& output) {
    output << "CREATE INDEX " << index_name << " ON /* table_name */ (/* columns */);";
    return true;
}

// Helper functions
std::string SBDatabaseEnhanced::ddlTypeToString(SBEnhanced::DDLType type) const {
    switch (type) {
        case SBEnhanced::DDLType::TABLE: return "TABLE";
        case SBEnhanced::DDLType::VIEW: return "VIEW";
        case SBEnhanced::DDLType::PROCEDURE: return "PROCEDURE";
        case SBEnhanced::DDLType::FUNCTION: return "FUNCTION";
        case SBEnhanced::DDLType::TRIGGER: return "TRIGGER";
        case SBEnhanced::DDLType::DOMAIN: return "DOMAIN";
        case SBEnhanced::DDLType::EXCEPTION: return "EXCEPTION";
        case SBEnhanced::DDLType::GENERATOR: return "GENERATOR";
        case SBEnhanced::DDLType::ROLE: return "ROLE";
        case SBEnhanced::DDLType::INDEX: return "INDEX";
        case SBEnhanced::DDLType::CONSTRAINT: return "CONSTRAINT";
        case SBEnhanced::DDLType::GRANT: return "GRANT";
        case SBEnhanced::DDLType::PACKAGE: return "PACKAGE";
        case SBEnhanced::DDLType::DATABASE: return "DATABASE";
        default: return "UNKNOWN";
    }
}

bool SBDatabaseEnhanced::getObjectNamesByType(SBEnhanced::DDLType type, std::vector<std::string>& names) {
    std::string sql;
    
    switch (type) {
        case SBEnhanced::DDLType::TABLE:
            sql = "SELECT RDB$RELATION_NAME FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0 AND RDB$VIEW_BLR IS NULL";
            break;
        case SBEnhanced::DDLType::VIEW:
            sql = "SELECT RDB$RELATION_NAME FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0 AND RDB$VIEW_BLR IS NOT NULL";
            break;
        case SBEnhanced::DDLType::PROCEDURE:
            sql = "SELECT RDB$PROCEDURE_NAME FROM RDB$PROCEDURES WHERE RDB$SYSTEM_FLAG = 0";
            break;
        case SBEnhanced::DDLType::FUNCTION:
            sql = "SELECT RDB$FUNCTION_NAME FROM RDB$FUNCTIONS WHERE RDB$SYSTEM_FLAG = 0";
            break;
        case SBEnhanced::DDLType::TRIGGER:
            sql = "SELECT RDB$TRIGGER_NAME FROM RDB$TRIGGERS WHERE RDB$SYSTEM_FLAG = 0";
            break;
        case SBEnhanced::DDLType::DOMAIN:
            sql = "SELECT RDB$FIELD_NAME FROM RDB$FIELDS WHERE RDB$SYSTEM_FLAG = 0 AND RDB$FIELD_NAME NOT STARTING WITH 'RDB$'";
            break;
        case SBEnhanced::DDLType::EXCEPTION:
            sql = "SELECT RDB$EXCEPTION_NAME FROM RDB$EXCEPTIONS WHERE RDB$SYSTEM_FLAG = 0";
            break;
        case SBEnhanced::DDLType::GENERATOR:
            sql = "SELECT RDB$GENERATOR_NAME FROM RDB$GENERATORS WHERE RDB$SYSTEM_FLAG = 0";
            break;
        case SBEnhanced::DDLType::ROLE:
            sql = "SELECT RDB$ROLE_NAME FROM RDB$ROLES WHERE RDB$SYSTEM_FLAG = 0";
            break;
        case SBEnhanced::DDLType::INDEX:
            sql = "SELECT RDB$INDEX_NAME FROM RDB$INDICES WHERE RDB$SYSTEM_FLAG = 0";
            break;
        default:
            return false;
    }
    
    std::vector<std::vector<std::string>> results;
    std::vector<std::string> columns;
    
    if (executeSelect(sql, results, columns)) {
        names.clear();
        for (const auto& row : results) {
            if (!row.empty()) {
                names.push_back(trim(row[0]));
            }
        }
        return true;
    }
    
    return false;
}

std::string SBDatabaseEnhanced::dependencyTypeToString(int type) const {
    switch (type) {
        case 0: return "TABLE";
        case 1: return "VIEW";
        case 2: return "TRIGGER";
        case 3: return "COMPUTED_FIELD";
        case 4: return "VALIDATION";
        case 5: return "PROCEDURE";
        case 6: return "EXPRESSION_INDEX";
        case 7: return "EXCEPTION";
        case 8: return "USER";
        case 9: return "FIELD";
        case 10: return "INDEX";
        case 11: return "USER_GROUP";
        case 12: return "ROLE";
        case 13: return "GENERATOR";
        case 14: return "UDF";
        case 15: return "BLOB_FILTER";
        case 16: return "COLLATION";
        case 17: return "PACKAGE";
        case 18: return "FUNCTION";
        default: return "UNKNOWN";
    }
}

std::string SBDatabaseEnhanced::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(start, end - start + 1);
}

std::string SBDatabaseEnhanced::serializeDependencies(const std::vector<SBEnhanced::ObjectDependency>& deps) const {
    std::ostringstream oss;
    for (const auto& dep : deps) {
        oss << dep.object_name << "|" << dep.object_type << "|" << dep.dependent_name << "|" << dep.dependent_type << "|" << dep.dependency_type << "|" << dep.dependency_level << "\n";
    }
    return oss.str();
}

bool SBDatabaseEnhanced::deserializeDependencies(const std::string& data, std::vector<SBEnhanced::ObjectDependency>& deps) const {
    deps.clear();
    std::istringstream iss(data);
    std::string line;
    
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        std::istringstream line_stream(line);
        std::string token;
        std::vector<std::string> tokens;
        
        while (std::getline(line_stream, token, '|')) {
            tokens.push_back(token);
        }
        
        if (tokens.size() >= 6) {
            SBEnhanced::ObjectDependency dep;
            dep.object_name = tokens[0];
            dep.object_type = tokens[1];
            dep.dependent_name = tokens[2];
            dep.dependent_type = tokens[3];
            dep.dependency_type = tokens[4];
            dep.dependency_level = std::stoi(tokens[5]);
            deps.push_back(dep);
        }
    }
    
    return true;
}
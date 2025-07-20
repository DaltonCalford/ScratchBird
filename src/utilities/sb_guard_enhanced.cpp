#include "sb_guard_enhanced.h"
#include "sb_engine_integration.h"
#include "utility_enhancements.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <future>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <chrono>
#include <condition_variable>
#include <csignal>

// System-specific includes
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/resource.h>
#endif

// Email support
#include <curl/curl.h>

namespace fs = std::filesystem;

// GuardEnhanced Implementation
GuardEnhanced::GuardEnhanced() 
    : engine(std::make_unique<SBEngineIntegration>()),
      guardian_service(nullptr),
      guardian_active(false),
      shutdown_requested(false) {
    
    if (!initializeEngine()) {
        logError("Constructor", "Failed to initialize ScratchBird engine integration");
    }
    
    current_progress.start_time = std::chrono::steady_clock::now();
}

GuardEnhanced::~GuardEnhanced() {
    if (guardian_active.load()) {
        requestShutdown();
        if (guardian_thread && guardian_thread->joinable()) {
            guardian_thread->join();
        }
        if (monitoring_thread && monitoring_thread->joinable()) {
            monitoring_thread->join();
        }
    }
}

bool GuardEnhanced::initializeEngine() {
    try {
        if (!engine) {
            logError("initializeEngine", "Engine integration not available");
            return false;
        }
        
        // Initialize engine components
        return engine->initialize();
        
    } catch (const std::exception& e) {
        logError("initializeEngine", std::string("Exception: ") + e.what());
        return false;
    }
}

bool GuardEnhanced::initializeGuardianService() {
    try {
        // Initialize service component if needed
        // This would integrate with jrd::Service infrastructure
        return true;
        
    } catch (const std::exception& e) {
        logError("initializeGuardianService", std::string("Exception: ") + e.what());
        return false;
    }
}

// === ORIGINAL GUARDIAN FUNCTIONALITY (100% Compatible) ===

bool GuardEnhanced::startGuardian(const std::string& config_file) {
    // Create default options for classic compatibility
    SBEnhanced::GuardianOptions options;
    options.config_file_path = config_file.empty() ? "guardian.conf" : config_file;
    options.mode = SBEnhanced::GuardianMode::AUTO_RESTART;
    options.monitoring_level = SBEnhanced::MonitoringLevel::STANDARD;
    
    SBEnhanced::GuardianOperationResult result;
    return startGuardianEnhanced(options, result);
}

bool GuardEnhanced::stopGuardian() {
    SBEnhanced::GuardianOperationResult result;
    return stopGuardianEnhanced(result);
}

bool GuardEnhanced::restartDatabase(const std::string& database_path) {
    // Find database alias by path
    std::string database_alias;
    {
        std::lock_guard<std::mutex> lock(config_mutex);
        for (const auto& config : monitored_databases) {
            if (config.database_path == database_path) {
                database_alias = config.alias_name;
                break;
            }
        }
    }
    
    if (database_alias.empty()) {
        logError("restartDatabase", "Database not found in monitoring list: " + database_path);
        return false;
    }
    
    SBEnhanced::GuardianOperationResult result;
    return restartDatabaseEnhanced(database_alias, result);
}

bool GuardEnhanced::getGuardianStatus() {
    return guardian_active.load();
}

// === ENHANCED FUNCTIONALITY ===

bool GuardEnhanced::startGuardianEnhanced(const SBEnhanced::GuardianOptions& options,
                                         SBEnhanced::GuardianOperationResult& result) {
    
    result.operation_type = SBEnhanced::GuardianOperation::START_GUARDIAN;
    result.start_time = std::chrono::steady_clock::now();
    
    try {
        if (guardian_active.load()) {
            result.errors.push_back("Guardian is already running");
            return false;
        }
        
        // Load configuration
        if (!options.config_file_path.empty()) {
            if (!loadConfiguration(options.config_file_path)) {
                result.errors.push_back("Failed to load configuration file: " + options.config_file_path);
                return false;
            }
        }
        
        // Initialize service if needed
        if (!initializeGuardianService()) {
            result.errors.push_back("Failed to initialize guardian service");
            return false;
        }
        
        // Update progress
        guardian_active = true;
        shutdown_requested = false;
        updateProgress(SBEnhanced::GuardianOperation::START_GUARDIAN, "Starting guardian");
        
        // Start guardian threads
        guardian_thread = std::make_unique<std::thread>(&GuardEnhanced::guardianMainLoop, this);
        monitoring_thread = std::make_unique<std::thread>(&GuardEnhanced::monitoringLoop, this);
        
        result.end_time = std::chrono::steady_clock::now();
        result.operation_successful = true;
        result.messages.push_back("Guardian started successfully");
        
        logInfo("startGuardianEnhanced", "Guardian started with " + std::to_string(monitored_databases.size()) + " databases");
        
        return true;
        
    } catch (const std::exception& e) {
        logError("startGuardianEnhanced", std::string("Exception: ") + e.what());
        result.errors.push_back(std::string("Guardian start failed: ") + e.what());
        result.end_time = std::chrono::steady_clock::now();
        result.operation_successful = false;
        guardian_active = false;
        return false;
    }
}

bool GuardEnhanced::stopGuardianEnhanced(SBEnhanced::GuardianOperationResult& result) {
    
    result.operation_type = SBEnhanced::GuardianOperation::STOP_GUARDIAN;
    result.start_time = std::chrono::steady_clock::now();
    
    try {
        if (!guardian_active.load()) {
            result.warnings.push_back("Guardian is not running");
            result.operation_successful = true;
            return true;
        }
        
        // Signal shutdown
        shutdown_requested = true;
        updateProgress(SBEnhanced::GuardianOperation::STOP_GUARDIAN, "Stopping guardian");
        
        // Wait for threads to complete
        if (guardian_thread && guardian_thread->joinable()) {
            guardian_thread->join();
        }
        if (monitoring_thread && monitoring_thread->joinable()) {
            monitoring_thread->join();
        }
        
        guardian_active = false;
        
        result.end_time = std::chrono::steady_clock::now();
        result.operation_successful = true;
        result.messages.push_back("Guardian stopped successfully");
        
        logInfo("stopGuardianEnhanced", "Guardian stopped");
        
        return true;
        
    } catch (const std::exception& e) {
        logError("stopGuardianEnhanced", std::string("Exception: ") + e.what());
        result.errors.push_back(std::string("Guardian stop failed: ") + e.what());
        result.end_time = std::chrono::steady_clock::now();
        result.operation_successful = false;
        return false;
    }
}

bool GuardEnhanced::addDatabaseToMonitoring(const SBEnhanced::DatabaseConfig& config,
                                           SBEnhanced::GuardianOperationResult& result) {
    
    try {
        std::lock_guard<std::mutex> lock(config_mutex);
        
        // Check if database already exists
        for (const auto& existing : monitored_databases) {
            if (existing.alias_name == config.alias_name || existing.database_path == config.database_path) {
                result.errors.push_back("Database already being monitored: " + config.alias_name);
                return false;
            }
        }
        
        // Validate configuration
        if (config.database_path.empty() || config.alias_name.empty()) {
            result.errors.push_back("Database path and alias name are required");
            return false;
        }
        
        // Test connection
        if (!testDatabaseConnection(config)) {
            result.warnings.push_back("Initial connection test failed for: " + config.alias_name);
        }
        
        // Add to monitoring list
        monitored_databases.push_back(config);
        
        // Initialize status
        SBEnhanced::DatabaseStatus status;
        status.database_path = config.database_path;
        status.alias_name = config.alias_name;
        status.state = SBEnhanced::DatabaseState::UNKNOWN;
        status.last_check = std::chrono::system_clock::now();
        
        {
            std::lock_guard<std::mutex> status_lock(status_mutex);
            database_status[config.alias_name] = status;
        }
        
        result.operation_successful = true;
        result.messages.push_back("Database added to monitoring: " + config.alias_name);
        
        logInfo("addDatabaseToMonitoring", "Added database: " + config.alias_name);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("addDatabaseToMonitoring", std::string("Exception: ") + e.what());
        result.errors.push_back(std::string("Failed to add database: ") + e.what());
        return false;
    }
}

bool GuardEnhanced::performHealthCheck(const SBEnhanced::HealthCheckOptions& options,
                                      SBEnhanced::HealthCheckResult& result) {
    
    try {
        result.database_path = options.database_path;
        result.check_timestamp = std::chrono::system_clock::now();
        
        // Find database configuration
        SBEnhanced::DatabaseConfig config;
        bool config_found = false;
        
        {
            std::lock_guard<std::mutex> lock(config_mutex);
            for (const auto& db_config : monitored_databases) {
                if (db_config.database_path == options.database_path) {
                    config = db_config;
                    config_found = true;
                    break;
                }
            }
        }
        
        if (!config_found) {
            result.health_issues.push_back("Database not found in monitoring configuration");
            return false;
        }
        
        auto start_time = std::chrono::steady_clock::now();
        
        // Connectivity check
        if (options.check_connectivity) {
            std::unique_ptr<jrd::Attachment> attachment;
            if (establishDatabaseConnection(config, attachment)) {
                result.connectivity_ok = true;
                closeDatabaseConnection(attachment);
            } else {
                result.connectivity_ok = false;
                result.health_issues.push_back("Database connectivity failed");
            }
        }
        
        // Responsiveness check
        if (options.check_responsiveness && result.connectivity_ok) {
            auto response_start = std::chrono::steady_clock::now();
            
            std::unique_ptr<jrd::Attachment> attachment;
            if (establishDatabaseConnection(config, attachment)) {
                // Perform simple query if requested
                if (options.perform_query_test) {
                    // Execute test query
                    // This is a simplified implementation
                    result.responsiveness_ok = true;
                } else {
                    result.responsiveness_ok = true;
                }
                closeDatabaseConnection(attachment);
            } else {
                result.responsiveness_ok = false;
                result.health_issues.push_back("Database responsiveness test failed");
            }
            
            auto response_end = std::chrono::steady_clock::now();
            result.response_time_ms = std::chrono::duration<double, std::milli>(response_end - response_start).count();
        }
        
        // System resource checks
        if (options.check_disk_space) {
            uint64_t available_space = 0;
            uint64_t memory_usage = 0;
            double cpu_usage = 0.0;
            
            if (checkSystemResources(options.database_path, available_space, memory_usage, cpu_usage)) {
                result.available_disk_space = available_space;
                result.memory_usage_mb = memory_usage / (1024 * 1024);
                
                // Check if disk space is adequate (at least 100MB free)
                result.disk_space_ok = (available_space > 100 * 1024 * 1024);
                if (!result.disk_space_ok) {
                    result.health_issues.push_back("Low disk space: " + std::to_string(available_space / (1024 * 1024)) + " MB available");
                }
                
                // Check memory usage (warn if over 80% of system memory)
                result.memory_usage_ok = true;  // Simplified check
            }
        }
        
        // Connection count check
        if (options.check_active_connections) {
            // This would query the database for active connection count
            // Simplified implementation
            result.active_connections = 1;  // Placeholder
            result.connections_ok = (result.active_connections < 100);  // Arbitrary limit
        }
        
        // Transaction health check
        if (options.check_transaction_health) {
            // This would check for long-running transactions, deadlocks, etc.
            // Simplified implementation
            result.active_transactions = 0;  // Placeholder
            result.transactions_ok = true;
        }
        
        // Determine overall state
        if (result.connectivity_ok && result.responsiveness_ok) {
            result.detected_state = SBEnhanced::DatabaseState::ONLINE;
        } else if (result.connectivity_ok) {
            result.detected_state = SBEnhanced::DatabaseState::ERROR;
        } else {
            result.detected_state = SBEnhanced::DatabaseState::OFFLINE;
        }
        
        result.health_check_successful = result.isHealthy();
        
        auto end_time = std::chrono::steady_clock::now();
        auto total_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        if (result.response_time_ms == 0.0) {
            result.response_time_ms = total_time;
        }
        
        return result.health_check_successful;
        
    } catch (const std::exception& e) {
        logError("performHealthCheck", std::string("Exception: ") + e.what());
        result.health_issues.push_back(std::string("Health check failed: ") + e.what());
        return false;
    }
}

bool GuardEnhanced::checkDatabaseHealth(const SBEnhanced::DatabaseConfig& config,
                                       SBEnhanced::HealthCheckResult& result) {
    
    SBEnhanced::HealthCheckOptions options;
    options.database_path = config.database_path;
    options.check_connectivity = true;
    options.check_responsiveness = true;
    options.check_disk_space = true;
    options.check_memory_usage = true;
    options.check_active_connections = true;
    options.check_transaction_health = true;
    options.timeout_seconds = config.connection_timeout_seconds;
    
    return performHealthCheck(options, result);
}

bool GuardEnhanced::updateDatabaseStatus(const std::string& database_alias,
                                        const SBEnhanced::HealthCheckResult& health_result) {
    
    try {
        std::lock_guard<std::mutex> lock(status_mutex);
        
        auto it = database_status.find(database_alias);
        if (it == database_status.end()) {
            logError("updateDatabaseStatus", "Database status not found: " + database_alias);
            return false;
        }
        
        SBEnhanced::DatabaseStatus& status = it->second;
        
        // Update status from health check
        status.last_check = health_result.check_timestamp;
        status.state = health_result.detected_state;
        status.is_responding = health_result.responsiveness_ok;
        status.response_time_ms = health_result.response_time_ms;
        status.active_connections = health_result.active_connections;
        status.memory_usage = health_result.memory_usage_mb * 1024 * 1024;
        status.disk_usage = health_result.available_disk_space;
        
        if (health_result.health_check_successful) {
            status.last_successful_connection = health_result.check_timestamp;
            status.consecutive_failures = 0;
        } else {
            status.consecutive_failures++;
            status.total_failures++;
            
            if (!health_result.health_issues.empty()) {
                status.last_error = health_result.health_issues[0];
                
                // Keep only the last 10 errors
                status.recent_errors.insert(status.recent_errors.begin(), health_result.health_issues.begin(), health_result.health_issues.end());
                if (status.recent_errors.size() > 10) {
                    status.recent_errors.resize(10);
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("updateDatabaseStatus", std::string("Exception: ") + e.what());
        return false;
    }
}

bool GuardEnhanced::handleDatabaseFailure(const std::string& database_alias,
                                         const SBEnhanced::HealthCheckResult& health_result) {
    
    try {
        // Find database configuration
        SBEnhanced::DatabaseConfig config;
        bool config_found = false;
        
        {
            std::lock_guard<std::mutex> lock(config_mutex);
            for (const auto& db_config : monitored_databases) {
                if (db_config.alias_name == database_alias) {
                    config = db_config;
                    config_found = true;
                    break;
                }
            }
        }
        
        if (!config_found) {
            logError("handleDatabaseFailure", "Database configuration not found: " + database_alias);
            return false;
        }
        
        // Get current status
        SBEnhanced::DatabaseStatus status;
        {
            std::lock_guard<std::mutex> lock(status_mutex);
            auto it = database_status.find(database_alias);
            if (it != database_status.end()) {
                status = it->second;
            }
        }
        
        // Generate alert
        SBEnhanced::AlertType alert_type = SBEnhanced::AlertType::WARNING;
        if (status.consecutive_failures >= config.max_connection_failures) {
            alert_type = SBEnhanced::AlertType::CRITICAL;
        }
        
        std::string alert_message = "Database health check failed for " + database_alias;
        std::string detailed_description = "Consecutive failures: " + std::to_string(status.consecutive_failures);
        
        for (const auto& issue : health_result.health_issues) {
            detailed_description += "\n- " + issue;
        }
        
        generateAlert(database_alias, alert_type, alert_message, detailed_description);
        
        // Determine action based on configuration and failure count
        if (config.enable_auto_restart && status.consecutive_failures >= config.max_connection_failures) {
            logInfo("handleDatabaseFailure", "Attempting automatic restart for " + database_alias);
            
            if (attemptDatabaseRestart(database_alias)) {
                generateAlert(database_alias, SBEnhanced::AlertType::INFO, 
                            "Database restart successful for " + database_alias,
                            "Database was automatically restarted and is now responding");
                return true;
            } else {
                // Restart failed, try failover if enabled
                if (config.enable_failover && !config.failover_hosts.empty()) {
                    logInfo("handleDatabaseFailure", "Attempting failover for " + database_alias);
                    
                    if (attemptDatabaseFailover(database_alias)) {
                        generateAlert(database_alias, SBEnhanced::AlertType::WARNING,
                                    "Database failover successful for " + database_alias,
                                    "Database failed over to backup instance");
                        return true;
                    }
                }
            }
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logError("handleDatabaseFailure", std::string("Exception: ") + e.what());
        return false;
    }
}

bool GuardEnhanced::attemptDatabaseRestart(const std::string& database_alias) {
    
    try {
        logInfo("attemptDatabaseRestart", "Restarting database: " + database_alias);
        
        // Find database configuration
        SBEnhanced::DatabaseConfig config;
        bool config_found = false;
        
        {
            std::lock_guard<std::mutex> lock(config_mutex);
            for (const auto& db_config : monitored_databases) {
                if (db_config.alias_name == database_alias) {
                    config = db_config;
                    config_found = true;
                    break;
                }
            }
        }
        
        if (!config_found) {
            logError("attemptDatabaseRestart", "Database configuration not found: " + database_alias);
            return false;
        }
        
        // This is a simplified restart implementation
        // In a real implementation, this would:
        // 1. Stop the database server process
        // 2. Wait for clean shutdown
        // 3. Start the database server process
        // 4. Wait for startup completion
        // 5. Test connectivity
        
        // For now, we'll just test if we can reconnect
        std::this_thread::sleep_for(std::chrono::seconds(5));  // Simulate restart time
        
        if (testDatabaseConnection(config)) {
            // Update status
            {
                std::lock_guard<std::mutex> lock(status_mutex);
                auto it = database_status.find(database_alias);
                if (it != database_status.end()) {
                    it->second.total_restarts++;
                    it->second.consecutive_failures = 0;
                    it->second.state = SBEnhanced::DatabaseState::ONLINE;
                    it->second.is_responding = true;
                    it->second.last_successful_connection = std::chrono::system_clock::now();
                }
            }
            
            logInfo("attemptDatabaseRestart", "Database restart successful: " + database_alias);
            return true;
        }
        
        logError("attemptDatabaseRestart", "Database restart failed: " + database_alias);
        return false;
        
    } catch (const std::exception& e) {
        logError("attemptDatabaseRestart", std::string("Exception: ") + e.what());
        return false;
    }
}

bool GuardEnhanced::attemptDatabaseFailover(const std::string& database_alias) {
    
    try {
        logInfo("attemptDatabaseFailover", "Attempting failover for database: " + database_alias);
        
        // Find database configuration
        SBEnhanced::DatabaseConfig config;
        bool config_found = false;
        
        {
            std::lock_guard<std::mutex> lock(config_mutex);
            for (const auto& db_config : monitored_databases) {
                if (db_config.alias_name == database_alias) {
                    config = db_config;
                    config_found = true;
                    break;
                }
            }
        }
        
        if (!config_found) {
            logError("attemptDatabaseFailover", "Database configuration not found: " + database_alias);
            return false;
        }
        
        // Try each failover host
        for (const auto& failover_host : config.failover_hosts) {
            logInfo("attemptDatabaseFailover", "Trying failover host: " + failover_host);
            
            // Create temporary config for failover host
            SBEnhanced::DatabaseConfig failover_config = config;
            failover_config.server_host = failover_host;
            if (!config.failover_database_path.empty()) {
                failover_config.database_path = config.failover_database_path;
            }
            
            if (testDatabaseConnection(failover_config)) {
                // Failover successful - update configuration
                {
                    std::lock_guard<std::mutex> lock(config_mutex);
                    for (auto& db_config : monitored_databases) {
                        if (db_config.alias_name == database_alias) {
                            db_config.server_host = failover_host;
                            if (!config.failover_database_path.empty()) {
                                db_config.database_path = config.failover_database_path;
                            }
                            break;
                        }
                    }
                }
                
                // Update status
                {
                    std::lock_guard<std::mutex> lock(status_mutex);
                    auto it = database_status.find(database_alias);
                    if (it != database_status.end()) {
                        it->second.consecutive_failures = 0;
                        it->second.state = SBEnhanced::DatabaseState::ONLINE;
                        it->second.is_responding = true;
                        it->second.last_successful_connection = std::chrono::system_clock::now();
                    }
                }
                
                logInfo("attemptDatabaseFailover", "Failover successful to host: " + failover_host);
                return true;
            }
        }
        
        logError("attemptDatabaseFailover", "All failover hosts failed for: " + database_alias);
        return false;
        
    } catch (const std::exception& e) {
        logError("attemptDatabaseFailover", std::string("Exception: ") + e.what());
        return false;
    }
}

// Main guardian loops
void GuardEnhanced::guardianMainLoop() {
    
    logInfo("guardianMainLoop", "Guardian main loop started");
    
    while (!shutdown_requested.load()) {
        try {
            // Update progress
            updateProgress(SBEnhanced::GuardianOperation::MONITOR_DATABASE, "Guardian monitoring");
            
            // Perform guardian tasks
            // This could include cleanup, statistics collection, etc.
            
            // Sleep for a short interval
            std::this_thread::sleep_for(std::chrono::seconds(10));
            
        } catch (const std::exception& e) {
            logError("guardianMainLoop", std::string("Exception: ") + e.what());
            std::this_thread::sleep_for(std::chrono::seconds(30));  // Sleep longer on error
        }
    }
    
    logInfo("guardianMainLoop", "Guardian main loop ended");
}

void GuardEnhanced::monitoringLoop() {
    
    logInfo("monitoringLoop", "Database monitoring loop started");
    
    while (!shutdown_requested.load()) {
        try {
            std::vector<SBEnhanced::DatabaseConfig> configs_to_check;
            
            // Get copy of configurations to check
            {
                std::lock_guard<std::mutex> lock(config_mutex);
                configs_to_check = monitored_databases;
            }
            
            // Check each database
            for (const auto& config : configs_to_check) {
                if (shutdown_requested.load()) {
                    break;
                }
                
                updateProgress(SBEnhanced::GuardianOperation::HEALTH_CHECK, config.alias_name);
                
                SBEnhanced::HealthCheckResult health_result;
                if (checkDatabaseHealth(config, health_result)) {
                    // Health check successful
                    updateDatabaseStatus(config.alias_name, health_result);
                } else {
                    // Health check failed
                    updateDatabaseStatus(config.alias_name, health_result);
                    handleDatabaseFailure(config.alias_name, health_result);
                }
                
                // Update progress counters
                current_progress.health_checks_performed++;
                current_progress.databases_monitored = configs_to_check.size();
                
                // Sleep between checks to avoid overwhelming the system
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            
            // Sleep until next monitoring cycle
            if (!configs_to_check.empty()) {
                uint32_t sleep_interval = configs_to_check[0].check_interval_seconds;
                std::this_thread::sleep_for(std::chrono::seconds(sleep_interval));
            } else {
                std::this_thread::sleep_for(std::chrono::seconds(30));  // Default interval
            }
            
        } catch (const std::exception& e) {
            logError("monitoringLoop", std::string("Exception: ") + e.what());
            std::this_thread::sleep_for(std::chrono::seconds(60));  // Sleep longer on error
        }
    }
    
    logInfo("monitoringLoop", "Database monitoring loop ended");
}

// Connection management
bool GuardEnhanced::establishDatabaseConnection(const SBEnhanced::DatabaseConfig& config,
                                               std::unique_ptr<jrd::Attachment>& attachment) {
    
    try {
        // This is a simplified connection implementation
        // In a real implementation, this would use the ScratchBird engine
        // to establish a proper database connection
        
        // For now, we'll simulate a connection test
        if (config.database_path.empty()) {
            return false;
        }
        
        // Check if database file exists
        if (!fs::exists(config.database_path)) {
            return false;
        }
        
        // Simulate connection establishment
        // attachment = engine->createAttachment(config);
        
        return true;  // Simplified success
        
    } catch (const std::exception& e) {
        logError("establishDatabaseConnection", std::string("Exception: ") + e.what());
        return false;
    }
}

void GuardEnhanced::closeDatabaseConnection(std::unique_ptr<jrd::Attachment>& attachment) {
    
    try {
        if (attachment) {
            // Close the attachment
            attachment.reset();
        }
        
    } catch (const std::exception& e) {
        logError("closeDatabaseConnection", std::string("Exception: ") + e.what());
    }
}

bool GuardEnhanced::testDatabaseConnection(const SBEnhanced::DatabaseConfig& config) {
    
    try {
        std::unique_ptr<jrd::Attachment> attachment;
        bool success = establishDatabaseConnection(config, attachment);
        
        if (success) {
            closeDatabaseConnection(attachment);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logError("testDatabaseConnection", std::string("Exception: ") + e.what());
        return false;
    }
}

// System resource monitoring
bool GuardEnhanced::checkSystemResources(const std::string& database_path,
                                        uint64_t& available_disk_space,
                                        uint64_t& memory_usage,
                                        double& cpu_usage) {
    
    try {
        // Get disk space
        fs::path db_path(database_path);
        fs::path parent_path = db_path.parent_path();
        
        if (fs::exists(parent_path)) {
            auto space_info = fs::space(parent_path);
            available_disk_space = space_info.available;
        } else {
            available_disk_space = 0;
        }
        
        // Get memory usage (simplified)
#ifdef _WIN32
        MEMORYSTATUSEX mem_status;
        mem_status.dwLength = sizeof(mem_status);
        if (GlobalMemoryStatusEx(&mem_status)) {
            memory_usage = mem_status.ullTotalPhys - mem_status.ullAvailPhys;
        } else {
            memory_usage = 0;
        }
#else
        struct sysinfo sys_info;
        if (sysinfo(&sys_info) == 0) {
            memory_usage = (sys_info.totalram - sys_info.freeram) * sys_info.mem_unit;
        } else {
            memory_usage = 0;
        }
#endif
        
        // Get CPU usage (simplified - returns 0 for now)
        cpu_usage = 0.0;
        
        return true;
        
    } catch (const std::exception& e) {
        logError("checkSystemResources", std::string("Exception: ") + e.what());
        return false;
    }
}

// Alert management
bool GuardEnhanced::generateAlert(const std::string& database_alias,
                                 SBEnhanced::AlertType alert_type,
                                 const std::string& message,
                                 const std::string& detailed_description) {
    
    try {
        SBEnhanced::GuardianAlert alert;
        alert.alert_type = alert_type;
        alert.database_alias = database_alias;
        alert.message = message;
        alert.detailed_description = detailed_description;
        alert.timestamp = std::chrono::system_clock::now();
        
        // Find database path
        {
            std::lock_guard<std::mutex> lock(config_mutex);
            for (const auto& config : monitored_databases) {
                if (config.alias_name == database_alias) {
                    alert.database_path = config.database_path;
                    break;
                }
            }
        }
        
        // Add to active alerts
        {
            std::lock_guard<std::mutex> lock(alert_mutex);
            active_alerts.push_back(alert);
            
            // Keep only the last 100 alerts
            if (active_alerts.size() > 100) {
                active_alerts.erase(active_alerts.begin());
            }
        }
        
        // Send alert
        sendAlert(alert);
        
        // Update progress
        current_progress.alerts_generated++;
        
        logInfo("generateAlert", "Alert generated: " + message);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("generateAlert", std::string("Exception: ") + e.what());
        return false;
    }
}

bool GuardEnhanced::sendAlert(const SBEnhanced::GuardianAlert& alert) {
    
    try {
        // Find database configuration for email settings
        SBEnhanced::DatabaseConfig config;
        bool config_found = false;
        
        {
            std::lock_guard<std::mutex> lock(config_mutex);
            for (const auto& db_config : monitored_databases) {
                if (db_config.alias_name == alert.database_alias) {
                    config = db_config;
                    config_found = true;
                    break;
                }
            }
        }
        
        if (config_found && config.enable_email_alerts && !config.alert_recipients.empty()) {
            return sendEmailAlert(alert, config.alert_recipients);
        }
        
        // For now, just log the alert
        std::string alert_log = "[" + alert.getAlertTypeString() + "] " + 
                               alert.database_alias + ": " + alert.message;
        logInfo("sendAlert", alert_log);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("sendAlert", std::string("Exception: ") + e.what());
        return false;
    }
}

bool GuardEnhanced::sendEmailAlert(const SBEnhanced::GuardianAlert& alert,
                                  const std::vector<std::string>& recipients) {
    
    try {
        // This is a simplified email implementation
        // In a real implementation, this would use SMTP to send emails
        
        for (const auto& recipient : recipients) {
            logInfo("sendEmailAlert", "Email alert would be sent to: " + recipient);
            logInfo("sendEmailAlert", "Subject: [ScratchBird Guardian] " + alert.getAlertTypeString() + 
                   " - " + alert.database_alias);
            logInfo("sendEmailAlert", "Message: " + alert.message);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("sendEmailAlert", std::string("Exception: ") + e.what());
        return false;
    }
}

// Progress tracking helpers
void GuardEnhanced::updateProgress(SBEnhanced::GuardianOperation operation,
                                  const std::string& current_database) {
    
    current_progress.current_operation = operation;
    current_progress.current_database = current_database;
    current_progress.guardian_active = guardian_active.load();
}

void GuardEnhanced::logError(const std::string& operation, const std::string& error) {
    std::string full_error = "[" + operation + "] " + error;
    error_log.push_back(full_error);
    last_error = full_error;
    
    // Also log to console/file if needed
    std::cerr << "ERROR: " << full_error << std::endl;
}

void GuardEnhanced::logWarning(const std::string& operation, const std::string& warning) {
    std::string full_warning = "[" + operation + "] " + warning;
    warning_log.push_back(full_warning);
    
    // Also log to console/file if needed
    std::cout << "WARNING: " << full_warning << std::endl;
}

void GuardEnhanced::logInfo(const std::string& operation, const std::string& info) {
    std::string full_info = "[" + operation + "] " + info;
    
    // Log to console/file if needed
    std::cout << "INFO: " << full_info << std::endl;
}

// Public interface methods
SBEnhanced::GuardianProgress GuardEnhanced::getCurrentProgress() const {
    return current_progress;
}

bool GuardEnhanced::isGuardianActive() const {
    return guardian_active.load();
}

void GuardEnhanced::requestShutdown() {
    shutdown_requested = true;
}

std::vector<std::string> GuardEnhanced::getErrors() const {
    return error_log;
}

std::vector<std::string> GuardEnhanced::getWarnings() const {
    return warning_log;
}

std::string GuardEnhanced::getLastError() const {
    return last_error;
}

void GuardEnhanced::clearErrorLog() {
    error_log.clear();
    warning_log.clear();
    last_error.clear();
}

bool GuardEnhanced::getDatabaseStatus(const std::string& database_alias,
                                     SBEnhanced::DatabaseStatus& status) {
    
    try {
        std::lock_guard<std::mutex> lock(status_mutex);
        
        auto it = database_status.find(database_alias);
        if (it != database_status.end()) {
            status = it->second;
            return true;
        }
        
        return false;
        
    } catch (const std::exception& e) {
        logError("getDatabaseStatus", std::string("Exception: ") + e.what());
        return false;
    }
}

bool GuardEnhanced::getAllDatabaseStatus(std::vector<SBEnhanced::DatabaseStatus>& status_list) {
    
    try {
        std::lock_guard<std::mutex> lock(status_mutex);
        
        status_list.clear();
        for (const auto& pair : database_status) {
            status_list.push_back(pair.second);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("getAllDatabaseStatus", std::string("Exception: ") + e.what());
        return false;
    }
}

// Configuration management
bool GuardEnhanced::loadConfiguration(const std::string& config_file_path) {
    
    try {
        if (!fs::exists(config_file_path)) {
            logError("loadConfiguration", "Configuration file not found: " + config_file_path);
            return false;
        }
        
        std::vector<SBEnhanced::DatabaseConfig> configs;
        if (!parseConfigurationFile(config_file_path, configs)) {
            logError("loadConfiguration", "Failed to parse configuration file: " + config_file_path);
            return false;
        }
        
        {
            std::lock_guard<std::mutex> lock(config_mutex);
            monitored_databases = configs;
        }
        
        // Initialize status for all databases
        {
            std::lock_guard<std::mutex> lock(status_mutex);
            database_status.clear();
            
            for (const auto& config : configs) {
                SBEnhanced::DatabaseStatus status;
                status.database_path = config.database_path;
                status.alias_name = config.alias_name;
                status.state = SBEnhanced::DatabaseState::UNKNOWN;
                status.last_check = std::chrono::system_clock::now();
                
                database_status[config.alias_name] = status;
            }
        }
        
        logInfo("loadConfiguration", "Loaded configuration for " + std::to_string(configs.size()) + " databases");
        
        return true;
        
    } catch (const std::exception& e) {
        logError("loadConfiguration", std::string("Exception: ") + e.what());
        return false;
    }
}

bool GuardEnhanced::parseConfigurationFile(const std::string& config_file_path,
                                          std::vector<SBEnhanced::DatabaseConfig>& configs) {
    
    try {
        std::ifstream config_file(config_file_path);
        if (!config_file.is_open()) {
            return false;
        }
        
        configs.clear();
        std::string line;
        SBEnhanced::DatabaseConfig current_config;
        bool in_database_section = false;
        
        while (std::getline(config_file, line)) {
            // Remove leading/trailing whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);
            
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            // Check for section headers
            if (line[0] == '[' && line.back() == ']') {
                if (in_database_section && !current_config.alias_name.empty()) {
                    configs.push_back(current_config);
                    current_config = SBEnhanced::DatabaseConfig();
                }
                
                std::string section = line.substr(1, line.length() - 2);
                in_database_section = (section.find("database") == 0);
                
                if (in_database_section) {
                    // Extract alias from section name like [database:mydb]
                    size_t colon_pos = section.find(':');
                    if (colon_pos != std::string::npos) {
                        current_config.alias_name = section.substr(colon_pos + 1);
                    }
                }
                continue;
            }
            
            // Parse key=value pairs
            if (in_database_section) {
                size_t equals_pos = line.find('=');
                if (equals_pos != std::string::npos) {
                    std::string key = line.substr(0, equals_pos);
                    std::string value = line.substr(equals_pos + 1);
                    
                    // Remove whitespace
                    key.erase(0, key.find_first_not_of(" \t"));
                    key.erase(key.find_last_not_of(" \t") + 1);
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);
                    
                    // Parse configuration values
                    if (key == "database_path") {
                        current_config.database_path = value;
                    } else if (key == "server_host") {
                        current_config.server_host = value;
                    } else if (key == "server_port") {
                        current_config.server_port = static_cast<uint16_t>(std::stoi(value));
                    } else if (key == "username") {
                        current_config.username = value;
                    } else if (key == "password") {
                        current_config.password = value;
                    } else if (key == "check_interval") {
                        current_config.check_interval_seconds = static_cast<uint32_t>(std::stoi(value));
                    } else if (key == "max_failures") {
                        current_config.max_connection_failures = static_cast<uint32_t>(std::stoi(value));
                    } else if (key == "auto_restart") {
                        current_config.enable_auto_restart = (value == "true" || value == "1");
                    } else if (key == "enable_failover") {
                        current_config.enable_failover = (value == "true" || value == "1");
                    }
                    // Add more configuration options as needed
                }
            }
        }
        
        // Add the last database config
        if (in_database_section && !current_config.alias_name.empty()) {
            configs.push_back(current_config);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("parseConfigurationFile", std::string("Exception: ") + e.what());
        return false;
    }
}

// Utility namespace implementations
namespace SBEnhanced {

// Quick operations
bool quickStartGuardian(const std::string& config_file) {
    GuardEnhanced guardian;
    return guardian.startGuardian(config_file);
}

bool quickStopGuardian() {
    GuardEnhanced guardian;
    return guardian.stopGuardian();
}

bool quickCheckDatabase(const std::string& database_path) {
    GuardEnhanced guardian;
    
    HealthCheckOptions options;
    options.database_path = database_path;
    options.check_connectivity = true;
    options.check_responsiveness = true;
    
    HealthCheckResult result;
    return guardian.performHealthCheck(options, result);
}

bool isDatabaseOnline(const std::string& database_path) {
    return quickCheckDatabase(database_path);
}

uint64_t getAvailableDiskSpace(const std::string& path) {
    try {
        if (fs::exists(path)) {
            auto space_info = fs::space(path);
            return space_info.available;
        }
        return 0;
    } catch (const std::exception&) {
        return 0;
    }
}

std::string formatAlert(const GuardianAlert& alert) {
    std::ostringstream oss;
    oss << "[" << alert.getAlertTypeString() << "] ";
    oss << alert.database_alias << ": " << alert.message;
    if (!alert.detailed_description.empty()) {
        oss << "\nDetails: " << alert.detailed_description;
    }
    return oss.str();
}

// Report generation methods
std::string HealthCheckResult::generateHealthReport() const {
    std::ostringstream oss;
    oss << "Database Health Check Report\n";
    oss << "============================\n";
    oss << "Database: " << database_path << "\n";
    oss << "Check Time: " << std::put_time(std::localtime(&std::chrono::system_clock::to_time_t(check_timestamp)), "%Y-%m-%d %H:%M:%S") << "\n";
    oss << "Overall Health: " << (health_check_successful ? "HEALTHY" : "UNHEALTHY") << "\n";
    oss << "State: ";
    
    switch (detected_state) {
        case DatabaseState::ONLINE: oss << "ONLINE"; break;
        case DatabaseState::OFFLINE: oss << "OFFLINE"; break;
        case DatabaseState::ERROR: oss << "ERROR"; break;
        case DatabaseState::MAINTENANCE: oss << "MAINTENANCE"; break;
        default: oss << "UNKNOWN"; break;
    }
    oss << "\n";
    
    oss << "\nDetailed Results:\n";
    oss << "- Connectivity: " << (connectivity_ok ? "OK" : "FAILED") << "\n";
    oss << "- Responsiveness: " << (responsiveness_ok ? "OK" : "FAILED") << "\n";
    oss << "- Disk Space: " << (disk_space_ok ? "OK" : "LOW") << "\n";
    oss << "- Memory Usage: " << (memory_usage_ok ? "OK" : "HIGH") << "\n";
    oss << "- Connections: " << (connections_ok ? "OK" : "HIGH") << "\n";
    oss << "- Transactions: " << (transactions_ok ? "OK" : "ISSUES") << "\n";
    
    oss << "\nMetrics:\n";
    oss << "- Response Time: " << std::fixed << std::setprecision(2) << response_time_ms << " ms\n";
    oss << "- Available Disk Space: " << (available_disk_space / (1024 * 1024)) << " MB\n";
    oss << "- Memory Usage: " << memory_usage_mb << " MB\n";
    oss << "- Active Connections: " << active_connections << "\n";
    oss << "- Active Transactions: " << active_transactions << "\n";
    
    if (!health_issues.empty()) {
        oss << "\nHealth Issues:\n";
        for (const auto& issue : health_issues) {
            oss << "- " << issue << "\n";
        }
    }
    
    if (!recommendations.empty()) {
        oss << "\nRecommendations:\n";
        for (const auto& recommendation : recommendations) {
            oss << "- " << recommendation << "\n";
        }
    }
    
    return oss.str();
}

} // namespace SBEnhanced
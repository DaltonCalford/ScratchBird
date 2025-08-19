#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

// Forward declarations for ScratchBird engine components
namespace jrd {
    class Attachment;
    class Database;
    class Transaction;
    class Service;
}

class SBEngineIntegration;

namespace SBEnhanced {

// Guardian operation types
enum class GuardianOperation {
    START_GUARDIAN = 0,
    STOP_GUARDIAN = 1,
    RESTART_DATABASE = 2,
    MONITOR_DATABASE = 3,
    HEALTH_CHECK = 4,
    FAILOVER = 5,
    BACKUP_DATABASE = 6
};

// Database states
enum class DatabaseState {
    UNKNOWN = 0,
    ONLINE = 1,
    OFFLINE = 2,
    SHUTTING_DOWN = 3,
    STARTING_UP = 4,
    ERROR = 5,
    MAINTENANCE = 6,
    BACKUP_IN_PROGRESS = 7
};

// Guardian modes
enum class GuardianMode {
    MONITOR_ONLY = 0,       // Monitor but don't take action
    AUTO_RESTART = 1,       // Automatically restart failed databases
    FAILOVER = 2,           // Failover to backup instances
    FULL_AUTO = 3           // Full automatic management
};

// Monitoring levels
enum class MonitoringLevel {
    BASIC = 0,              // Basic connection monitoring
    STANDARD = 1,           // Standard health checks
    COMPREHENSIVE = 2,      // Comprehensive monitoring
    ADVANCED = 3            // Advanced monitoring with analytics
};

// Alert types
enum class AlertType {
    INFO = 0,
    WARNING = 1,
    ERROR = 2,
    CRITICAL = 3
};

// Guardian progress tracking
struct GuardianProgress {
    GuardianOperation current_operation = GuardianOperation::MONITOR_DATABASE;
    uint64_t databases_monitored = 0;
    uint64_t health_checks_performed = 0;
    uint64_t restarts_performed = 0;
    uint64_t failovers_performed = 0;
    uint64_t alerts_generated = 0;
    std::string current_database;
    std::chrono::steady_clock::time_point start_time;
    bool guardian_active = false;
    
    std::chrono::seconds getUptime() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    }
};

// Database configuration
struct DatabaseConfig {
    std::string database_path;
    std::string alias_name;
    std::string server_host = "localhost";
    uint16_t server_port = 3050;
    std::string username = "SYSDBA";
    std::string password;
    std::string connection_string;
    
    // Monitoring settings
    uint32_t check_interval_seconds = 30;
    uint32_t connection_timeout_seconds = 10;
    uint32_t max_connection_failures = 3;
    bool enable_auto_restart = true;
    bool enable_failover = false;
    
    // Backup settings
    bool enable_auto_backup = false;
    std::string backup_directory;
    uint32_t backup_interval_hours = 24;
    uint32_t backup_retention_days = 7;
    
    // Failover settings
    std::vector<std::string> failover_hosts;
    std::string failover_database_path;
    uint32_t failover_timeout_seconds = 60;
    
    // Alert settings
    bool enable_email_alerts = false;
    std::vector<std::string> alert_recipients;
    std::string smtp_server;
    uint16_t smtp_port = 587;
    std::string smtp_username;
    std::string smtp_password;
    
    std::map<std::string, std::string> custom_properties;
};

// Database status information
struct DatabaseStatus {
    std::string database_path;
    std::string alias_name;
    DatabaseState state = DatabaseState::UNKNOWN;
    std::chrono::system_clock::time_point last_check;
    std::chrono::system_clock::time_point last_successful_connection;
    uint32_t consecutive_failures = 0;
    uint32_t total_failures = 0;
    uint32_t total_restarts = 0;
    bool is_responding = false;
    
    // Connection statistics
    uint32_t active_connections = 0;
    uint32_t total_connections = 0;
    uint64_t total_transactions = 0;
    
    // Performance metrics
    double cpu_usage = 0.0;
    uint64_t memory_usage = 0;
    uint64_t disk_usage = 0;
    double response_time_ms = 0.0;
    
    // Error information
    std::string last_error;
    std::vector<std::string> recent_errors;
    
    std::map<std::string, std::string> additional_metrics;
    
    bool isHealthy() const {
        return state == DatabaseState::ONLINE && is_responding && consecutive_failures == 0;
    }
    
    bool requiresAttention() const {
        return state == DatabaseState::ERROR || consecutive_failures > 0;
    }
};

// Guardian configuration options
struct GuardianOptions {
    GuardianMode mode = GuardianMode::AUTO_RESTART;
    MonitoringLevel monitoring_level = MonitoringLevel::STANDARD;
    std::string config_file_path;
    std::string log_file_path;
    std::string pid_file_path;
    
    // Global settings
    uint32_t global_check_interval_seconds = 30;
    uint32_t global_max_failures = 3;
    bool enable_logging = true;
    bool enable_alerts = true;
    bool run_as_daemon = false;
    
    // Performance settings
    uint32_t max_worker_threads = 4;
    uint32_t connection_pool_size = 10;
    uint32_t health_check_timeout_seconds = 30;
    
    // Advanced options
    bool enable_predictive_monitoring = false;
    bool enable_performance_analytics = false;
    bool enable_automated_optimization = false;
    
    std::function<void(const GuardianProgress&)> progress_callback;
};

// Health check options
struct HealthCheckOptions {
    std::string database_path;
    bool check_connectivity = true;
    bool check_responsiveness = true;
    bool check_disk_space = true;
    bool check_memory_usage = true;
    bool check_active_connections = true;
    bool check_transaction_health = true;
    bool perform_query_test = false;
    std::string test_query = "SELECT 1 FROM RDB$DATABASE";
    uint32_t timeout_seconds = 30;
};

// Guardian statistics
struct GuardianStatistics {
    std::chrono::steady_clock::time_point guardian_start_time;
    std::chrono::steady_clock::time_point last_update;
    
    // Monitoring statistics
    uint64_t total_health_checks = 0;
    uint64_t successful_checks = 0;
    uint64_t failed_checks = 0;
    uint64_t databases_monitored = 0;
    
    // Action statistics
    uint64_t databases_restarted = 0;
    uint64_t failovers_performed = 0;
    uint64_t backups_performed = 0;
    uint64_t alerts_sent = 0;
    
    // Performance statistics
    double average_check_time_ms = 0.0;
    double average_restart_time_ms = 0.0;
    uint64_t total_downtime_seconds = 0;
    
    // Error statistics
    uint64_t guardian_errors = 0;
    uint64_t configuration_errors = 0;
    uint64_t communication_errors = 0;
    
    std::vector<std::string> recent_actions;
    std::vector<std::string> error_log;
    
    std::chrono::seconds getUptime() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - guardian_start_time);
    }
    
    double getSuccessRate() const {
        if (total_health_checks == 0) return 0.0;
        return (static_cast<double>(successful_checks) / total_health_checks) * 100.0;
    }
};

// Guardian alert information
struct GuardianAlert {
    AlertType alert_type = AlertType::INFO;
    std::string database_alias;
    std::string database_path;
    std::string message;
    std::string detailed_description;
    std::chrono::system_clock::time_point timestamp;
    bool acknowledged = false;
    std::string acknowledged_by;
    std::chrono::system_clock::time_point acknowledged_time;
    std::map<std::string, std::string> alert_metadata;
    
    std::string getAlertTypeString() const {
        switch (alert_type) {
            case AlertType::INFO: return "INFO";
            case AlertType::WARNING: return "WARNING";
            case AlertType::ERROR: return "ERROR";
            case AlertType::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }
};

// Guardian operation result
struct GuardianOperationResult {
    GuardianOperation operation_type;
    bool operation_successful = false;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    std::vector<std::string> messages;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    GuardianStatistics detailed_stats;
    
    std::chrono::milliseconds getDuration() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    }
    
    std::string generateOperationReport() const;
    bool hasErrors() const { return !errors.empty(); }
    bool hasWarnings() const { return !warnings.empty(); }
};

// Health check result
struct HealthCheckResult {
    std::string database_path;
    bool health_check_successful = false;
    DatabaseState detected_state = DatabaseState::UNKNOWN;
    bool connectivity_ok = false;
    bool responsiveness_ok = false;
    bool disk_space_ok = false;
    bool memory_usage_ok = false;
    bool connections_ok = false;
    bool transactions_ok = false;
    
    // Detailed metrics
    double response_time_ms = 0.0;
    uint64_t available_disk_space = 0;
    uint64_t memory_usage_mb = 0;
    uint32_t active_connections = 0;
    uint32_t active_transactions = 0;
    
    std::vector<std::string> health_issues;
    std::vector<std::string> recommendations;
    std::chrono::system_clock::time_point check_timestamp;
    
    bool isHealthy() const {
        return health_check_successful && connectivity_ok && responsiveness_ok && 
               disk_space_ok && memory_usage_ok && connections_ok && transactions_ok;
    }
    
    bool requiresImmediateAttention() const {
        return !connectivity_ok || !responsiveness_ok;
    }
    
    std::string generateHealthReport() const;
};

} // namespace SBEnhanced

// Main enhanced Guardian utility class
class GuardEnhanced {
private:
    std::unique_ptr<SBEngineIntegration> engine;
    std::unique_ptr<jrd::Service> guardian_service;
    std::atomic<bool> guardian_active{false};
    std::atomic<bool> shutdown_requested{false};
    SBEnhanced::GuardianProgress current_progress;
    std::vector<std::string> error_log;
    std::vector<std::string> warning_log;
    std::string last_error;
    
    // Database management
    std::vector<SBEnhanced::DatabaseConfig> monitored_databases;
    std::map<std::string, SBEnhanced::DatabaseStatus> database_status;
    std::vector<SBEnhanced::GuardianAlert> active_alerts;
    
    // Threading
    std::unique_ptr<std::thread> guardian_thread;
    std::unique_ptr<std::thread> monitoring_thread;
    std::mutex status_mutex;
    std::mutex alert_mutex;
    std::mutex config_mutex;

public:
    // Constructor and destructor
    GuardEnhanced();
    ~GuardEnhanced();
    
    // === ORIGINAL GUARDIAN FUNCTIONALITY (100% Compatible) ===
    
    // Basic guardian operations
    bool startGuardian(const std::string& config_file = "");
    bool stopGuardian();
    bool restartDatabase(const std::string& database_path);
    bool getGuardianStatus();
    
    // === ENHANCED FUNCTIONALITY ===
    
    // Advanced guardian operations
    bool startGuardianEnhanced(const SBEnhanced::GuardianOptions& options,
                              SBEnhanced::GuardianOperationResult& result);
    
    bool stopGuardianEnhanced(SBEnhanced::GuardianOperationResult& result);
    
    bool addDatabaseToMonitoring(const SBEnhanced::DatabaseConfig& config,
                                SBEnhanced::GuardianOperationResult& result);
    
    bool removeDatabaseFromMonitoring(const std::string& database_alias,
                                     SBEnhanced::GuardianOperationResult& result);
    
    // Database management
    bool restartDatabaseEnhanced(const std::string& database_alias,
                                SBEnhanced::GuardianOperationResult& result);
    
    bool performFailover(const std::string& database_alias,
                        const std::string& target_host,
                        SBEnhanced::GuardianOperationResult& result);
    
    bool backupDatabase(const std::string& database_alias,
                       const std::string& backup_path = "",
                       SBEnhanced::GuardianOperationResult& result);
    
    // Health checking and monitoring
    bool performHealthCheck(const SBEnhanced::HealthCheckOptions& options,
                           SBEnhanced::HealthCheckResult& result);
    
    bool performComprehensiveHealthCheck(const std::string& database_alias,
                                        SBEnhanced::HealthCheckResult& result);
    
    bool getDatabaseStatus(const std::string& database_alias,
                          SBEnhanced::DatabaseStatus& status);
    
    bool getAllDatabaseStatus(std::vector<SBEnhanced::DatabaseStatus>& status_list);
    
    // Configuration management
    bool loadConfiguration(const std::string& config_file_path);
    bool saveConfiguration(const std::string& config_file_path);
    bool reloadConfiguration();
    
    bool updateDatabaseConfig(const std::string& database_alias,
                             const SBEnhanced::DatabaseConfig& new_config);
    
    bool getDatabaseConfig(const std::string& database_alias,
                          SBEnhanced::DatabaseConfig& config);
    
    // Alert management
    bool getActiveAlerts(std::vector<SBEnhanced::GuardianAlert>& alerts);
    bool acknowledgeAlert(const std::string& alert_id, const std::string& acknowledged_by);
    bool clearAlert(const std::string& alert_id);
    bool sendTestAlert(const std::string& recipient);
    
    // Statistics and reporting
    bool getGuardianStatistics(SBEnhanced::GuardianStatistics& stats);
    bool generateStatusReport(const std::string& report_format = "TEXT",
                             const std::string& output_path = "");
    
    bool generateHealthReport(const std::string& database_alias,
                             const std::string& report_format = "TEXT",
                             const std::string& output_path = "");
    
    // Advanced monitoring features
    bool enablePredictiveMonitoring(bool enable = true);
    bool enablePerformanceAnalytics(bool enable = true);
    bool enableAutomatedOptimization(bool enable = true);
    
    bool getPerformanceTrends(const std::string& database_alias,
                             const std::chrono::hours& time_window,
                             std::map<std::string, std::vector<double>>& trends);
    
    bool getPredictedIssues(const std::string& database_alias,
                           std::vector<std::string>& predicted_issues);
    
    // Progress monitoring
    SBEnhanced::GuardianProgress getCurrentProgress() const;
    bool isGuardianActive() const;
    void requestShutdown();
    
    // Error handling and logging
    std::vector<std::string> getErrors() const;
    std::vector<std::string> getWarnings() const;
    std::string getLastError() const;
    void clearErrorLog();
    
    // Utility functions
    bool testDatabaseConnection(const SBEnhanced::DatabaseConfig& config);
    bool validateConfiguration(const std::string& config_file_path,
                              std::vector<std::string>& validation_errors);

private:
    // Internal initialization
    bool initializeEngine();
    bool initializeGuardianService();
    
    // Main guardian loop
    void guardianMainLoop();
    void monitoringLoop();
    
    // Database monitoring helpers
    bool checkDatabaseHealth(const SBEnhanced::DatabaseConfig& config,
                            SBEnhanced::HealthCheckResult& result);
    
    bool updateDatabaseStatus(const std::string& database_alias,
                             const SBEnhanced::HealthCheckResult& health_result);
    
    bool handleDatabaseFailure(const std::string& database_alias,
                              const SBEnhanced::HealthCheckResult& health_result);
    
    bool attemptDatabaseRestart(const std::string& database_alias);
    bool attemptDatabaseFailover(const std::string& database_alias);
    
    // Alert system helpers
    bool generateAlert(const std::string& database_alias,
                      SBEnhanced::AlertType alert_type,
                      const std::string& message,
                      const std::string& detailed_description = "");
    
    bool sendAlert(const SBEnhanced::GuardianAlert& alert);
    bool sendEmailAlert(const SBEnhanced::GuardianAlert& alert,
                       const std::vector<std::string>& recipients);
    
    // Configuration helpers
    bool parseConfigurationFile(const std::string& config_file_path,
                               std::vector<SBEnhanced::DatabaseConfig>& configs);
    
    bool writeConfigurationFile(const std::string& config_file_path,
                               const std::vector<SBEnhanced::DatabaseConfig>& configs);
    
    // Connection management
    bool establishDatabaseConnection(const SBEnhanced::DatabaseConfig& config,
                                    std::unique_ptr<jrd::Attachment>& attachment);
    
    void closeDatabaseConnection(std::unique_ptr<jrd::Attachment>& attachment);
    
    // Performance monitoring helpers
    bool collectPerformanceMetrics(const std::string& database_alias,
                                  std::map<std::string, double>& metrics);
    
    bool analyzePerformanceTrends(const std::string& database_alias,
                                 std::vector<std::string>& analysis_results);
    
    // System resource monitoring
    bool checkSystemResources(const std::string& database_path,
                             uint64_t& available_disk_space,
                             uint64_t& memory_usage,
                             double& cpu_usage);
    
    // Backup helpers
    bool performAutomaticBackup(const SBEnhanced::DatabaseConfig& config);
    bool cleanupOldBackups(const std::string& backup_directory,
                          uint32_t retention_days);
    
    // Progress tracking helpers
    void updateProgress(SBEnhanced::GuardianOperation operation,
                       const std::string& current_database);
    
    void logError(const std::string& operation, const std::string& error);
    void logWarning(const std::string& operation, const std::string& warning);
    void logInfo(const std::string& operation, const std::string& info);
    
    // Statistics collection helpers
    void collectGuardianStatistics(SBEnhanced::GuardianStatistics& stats);
    void updateStatistics();
    
    // Thread safety helpers
    void lockStatus() { status_mutex.lock(); }
    void unlockStatus() { status_mutex.unlock(); }
    void lockAlerts() { alert_mutex.lock(); }
    void unlockAlerts() { alert_mutex.unlock(); }
    void lockConfig() { config_mutex.lock(); }
    void unlockConfig() { config_mutex.unlock(); }
};

// Utility functions for enhanced Guardian
namespace SBEnhanced {

// Quick guardian operations
bool quickStartGuardian(const std::string& config_file = "guardian.conf");
bool quickStopGuardian();
bool quickCheckDatabase(const std::string& database_path);

// Configuration helpers
bool createDefaultConfiguration(const std::string& config_file_path);
bool validateGuardianConfiguration(const std::string& config_file_path);

// Database utilities
bool isDatabaseOnline(const std::string& database_path);
bool getDatabaseConnections(const std::string& database_path, uint32_t& active_connections);

// System utilities
bool checkSystemHealth();
uint64_t getAvailableDiskSpace(const std::string& path);
uint64_t getMemoryUsage();
double getCPUUsage();

// Alert utilities
std::string formatAlert(const GuardianAlert& alert);
bool sendSystemNotification(const std::string& title, const std::string& message);

// Compatibility helpers for command-line usage
int parseGuardianCommandLine(int argc, char* argv[],
                            std::string& config_file,
                            GuardianOptions& guardian_opts);

bool executeClassicGuardianCommand(const std::string& command_line);

} // namespace SBEnhanced
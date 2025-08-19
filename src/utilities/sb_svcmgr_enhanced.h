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
#include <set>

// Forward declarations for ScratchBird engine components
namespace jrd {
    class Attachment;
    class Database;
    class Transaction;
    class Service;
}

class SBEngineIntegration;

namespace SBEnhanced {

// Service operation types
enum class ServiceOperation {
    BACKUP = 0,         // Database backup service
    RESTORE = 1,        // Database restore service
    REPAIR = 2,         // Database repair service
    VALIDATE = 3,       // Database validation service
    SWEEP = 4,          // Database sweep service
    STATISTICS = 5,     // Database statistics service
    USER_MANAGEMENT = 6, // User management service
    ROLE_MANAGEMENT = 7, // Role management service
    TRACE = 8,          // Trace service
    MONITORING = 9,     // Database monitoring service
    MAINTENANCE = 10,   // Database maintenance service
    CUSTOM = 255        // Custom service operation
};

// Service status types
enum class ServiceStatus {
    UNKNOWN = 0,        // Unknown status
    IDLE = 1,           // Service is idle
    STARTING = 2,       // Service is starting
    RUNNING = 3,        // Service is running
    PAUSING = 4,        // Service is pausing
    PAUSED = 5,         // Service is paused
    STOPPING = 6,       // Service is stopping
    STOPPED = 7,        // Service is stopped
    ERROR = 8,          // Service encountered error
    COMPLETED = 9,      // Service completed successfully
    CANCELLED = 10,     // Service was cancelled
    TIMEOUT = 11        // Service timed out
};

// Service priority levels
enum class ServicePriority {
    LOW = 0,            // Low priority
    NORMAL = 1,         // Normal priority
    HIGH = 2,           // High priority
    CRITICAL = 3        // Critical priority
};

// Service monitoring levels
enum class ServiceMonitoringLevel {
    NONE = 0,           // No monitoring
    BASIC = 1,          // Basic monitoring
    STANDARD = 2,       // Standard monitoring
    DETAILED = 3,       // Detailed monitoring
    COMPREHENSIVE = 4   // Comprehensive monitoring
};

// Service execution modes
enum class ServiceExecutionMode {
    SYNCHRONOUS = 0,    // Synchronous execution
    ASYNCHRONOUS = 1,   // Asynchronous execution
    SCHEDULED = 2,      // Scheduled execution
    TRIGGERED = 3       // Event-triggered execution
};

// Service information structure
struct ServiceInfo {
    uint64_t service_id = 0;
    std::string service_name;
    ServiceOperation operation = ServiceOperation::CUSTOM;
    ServiceStatus status = ServiceStatus::UNKNOWN;
    ServicePriority priority = ServicePriority::NORMAL;
    ServiceExecutionMode execution_mode = ServiceExecutionMode::SYNCHRONOUS;
    
    std::string database_path;
    std::string user_name;
    std::string server_name;
    uint32_t server_port = 3050;
    
    std::chrono::system_clock::time_point creation_time;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::chrono::milliseconds execution_duration{0};
    
    double progress_percentage = 0.0;
    std::string current_operation;
    std::string current_file;
    
    uint64_t bytes_processed = 0;
    uint64_t total_bytes = 0;
    uint32_t items_processed = 0;
    uint32_t total_items = 0;
    
    bool is_cancellable = true;
    bool is_pausable = false;
    bool is_resumable = false;
    bool auto_retry = false;
    uint32_t retry_count = 0;
    uint32_t max_retries = 3;
    
    std::vector<std::string> service_parameters;
    std::map<std::string, std::string> service_options;
    std::vector<std::string> output_messages;
    std::vector<std::string> error_messages;
    std::vector<std::string> warning_messages;
    
    std::string getServiceOperationString() const;
    std::string getServiceStatusString() const;
    std::string getServicePriorityString() const;
    std::string getExecutionModeString() const;
    bool isRunning() const { return status == ServiceStatus::RUNNING; }
    bool isCompleted() const { return status == ServiceStatus::COMPLETED; }
    bool hasErrors() const { return !error_messages.empty(); }
    std::chrono::milliseconds getExecutionDuration() const;
    double getProcessingRate() const;
};

// Service queue information
struct ServiceQueue {
    std::string queue_name;
    ServicePriority queue_priority = ServicePriority::NORMAL;
    uint32_t max_concurrent_services = 5;
    uint32_t max_queue_size = 100;
    
    std::vector<ServiceInfo> queued_services;
    std::vector<ServiceInfo> running_services;
    std::vector<ServiceInfo> completed_services;
    
    std::chrono::system_clock::time_point queue_creation_time;
    uint64_t total_services_processed = 0;
    uint64_t total_services_failed = 0;
    double average_execution_time_ms = 0.0;
    
    bool is_active = true;
    bool accept_new_services = true;
    uint32_t current_load_percentage = 0;
    
    uint32_t getQueuedCount() const { return static_cast<uint32_t>(queued_services.size()); }
    uint32_t getRunningCount() const { return static_cast<uint32_t>(running_services.size()); }
    uint32_t getCompletedCount() const { return static_cast<uint32_t>(completed_services.size()); }
    bool isFull() const { return queued_services.size() >= max_queue_size; }
    bool canAcceptService() const { return accept_new_services && !isFull(); }
};

// Service configuration
struct ServiceConfiguration {
    std::string configuration_name;
    std::string server_host = "localhost";
    uint32_t server_port = 3050;
    std::string database_path;
    std::string user_name = "SYSDBA";
    std::string password;
    std::string role_name;
    
    // Connection settings
    uint32_t connection_timeout_seconds = 30;
    uint32_t operation_timeout_seconds = 3600;  // 1 hour
    uint32_t retry_interval_seconds = 5;
    uint32_t max_retry_attempts = 3;
    bool auto_reconnect = true;
    
    // Service execution settings
    ServiceExecutionMode default_execution_mode = ServiceExecutionMode::ASYNCHRONOUS;
    ServicePriority default_priority = ServicePriority::NORMAL;
    ServiceMonitoringLevel monitoring_level = ServiceMonitoringLevel::STANDARD;
    uint32_t progress_update_interval_ms = 1000;
    
    // Queue settings
    uint32_t max_concurrent_services = 5;
    uint32_t max_queue_size = 100;
    bool enable_service_queuing = true;
    bool enable_priority_scheduling = true;
    
    // Logging and monitoring
    bool enable_detailed_logging = false;
    bool enable_performance_tracking = true;
    bool enable_service_history = true;
    uint32_t history_retention_days = 30;
    std::string log_file_path;
    
    // Security settings
    bool require_authentication = true;
    bool enable_role_based_access = false;
    std::vector<std::string> allowed_operations;
    std::vector<std::string> restricted_operations;
    
    // Advanced settings
    uint32_t service_thread_pool_size = 10;
    uint32_t io_buffer_size_kb = 64;
    bool enable_compression = false;
    bool enable_encryption = false;
    
    std::map<std::string, std::string> custom_parameters;
    
    bool isValidConfiguration() const;
    std::string validateConfiguration() const;
};

// Service options for operations
struct ServiceOptions {
    ServiceOperation operation = ServiceOperation::CUSTOM;
    ServiceExecutionMode execution_mode = ServiceExecutionMode::ASYNCHRONOUS;
    ServicePriority priority = ServicePriority::NORMAL;
    
    std::string service_name;
    std::vector<std::string> service_parameters;
    std::map<std::string, std::string> service_options;
    
    // Execution settings
    uint32_t timeout_seconds = 3600;
    bool auto_retry = false;
    uint32_t max_retries = 3;
    uint32_t retry_interval_seconds = 5;
    
    // Monitoring settings
    bool enable_progress_monitoring = true;
    bool enable_detailed_logging = false;
    uint32_t progress_update_interval_ms = 1000;
    
    // Callback functions
    std::function<void(const ServiceInfo&)> progress_callback;
    std::function<void(const ServiceInfo&)> completion_callback;
    std::function<void(const ServiceInfo&, const std::string&)> error_callback;
    std::function<void(const std::string&)> log_callback;
    
    // Output options
    bool capture_output = true;
    bool capture_errors = true;
    bool capture_warnings = true;
    std::string output_file_path;
    
    // Advanced options
    bool run_in_background = false;
    bool persist_across_sessions = false;
    uint64_t estimated_duration_ms = 0;
    std::string scheduled_start_time;  // ISO format
    
    std::map<std::string, std::string> custom_options;
};

// Service statistics
struct ServiceStatistics {
    std::chrono::system_clock::time_point collection_time;
    std::string statistics_scope;  // "global", "queue", "service"
    
    // Overall service counts
    uint64_t total_services_started = 0;
    uint64_t total_services_completed = 0;
    uint64_t total_services_failed = 0;
    uint64_t total_services_cancelled = 0;
    uint32_t currently_running = 0;
    uint32_t currently_queued = 0;
    
    // Service type breakdown
    std::map<ServiceOperation, uint64_t> services_by_operation;
    std::map<ServiceStatus, uint64_t> services_by_status;
    std::map<ServicePriority, uint64_t> services_by_priority;
    
    // Performance metrics
    double average_execution_time_ms = 0.0;
    double median_execution_time_ms = 0.0;
    double maximum_execution_time_ms = 0.0;
    double minimum_execution_time_ms = 0.0;
    double average_queue_wait_time_ms = 0.0;
    
    // Throughput metrics
    double services_per_hour = 0.0;
    double services_per_minute = 0.0;
    double bytes_processed_per_second = 0.0;
    double peak_concurrent_services = 0.0;
    
    // Resource utilization
    double average_cpu_usage = 0.0;
    double average_memory_usage_mb = 0.0;
    double average_disk_io_mbps = 0.0;
    double average_network_io_mbps = 0.0;
    
    // Error and reliability metrics
    double success_rate = 0.0;
    double failure_rate = 0.0;
    double cancellation_rate = 0.0;
    double retry_rate = 0.0;
    uint64_t total_retries = 0;
    
    // Queue statistics
    std::vector<std::string> most_used_queues;
    std::vector<std::string> slowest_operations;
    std::vector<std::string> most_error_prone_operations;
    
    // Efficiency metrics
    double overall_efficiency = 0.0;
    double queue_utilization = 0.0;
    double resource_efficiency = 0.0;
    
    std::string generateStatisticsReport() const;
    std::string generatePerformanceReport() const;
};

// Service monitoring progress
struct ServiceMonitoringProgress {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point current_time;
    
    uint64_t services_monitored = 0;
    uint64_t services_started = 0;
    uint64_t services_completed = 0;
    uint64_t services_failed = 0;
    uint64_t alerts_generated = 0;
    
    std::string current_monitoring_scope;
    std::string current_operation;
    bool monitoring_active = false;
    
    std::chrono::seconds getMonitoringDuration() const;
    double getServicesPerMinute() const;
    double getFailureRate() const;
    std::string getProgressSummary() const;
};

// Service management result
struct ServiceManagementResult {
    ServiceConfiguration service_config;
    ServiceMonitoringProgress monitoring_progress;
    
    bool operation_successful = false;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    
    // Operation results
    std::vector<uint64_t> created_service_ids;
    std::vector<uint64_t> modified_service_ids;
    std::vector<uint64_t> cancelled_service_ids;
    std::vector<ServiceInfo> service_results;
    
    // Performance metrics
    uint32_t services_processed = 0;
    uint32_t services_succeeded = 0;
    uint32_t services_failed = 0;
    std::chrono::milliseconds total_execution_time{0};
    double average_service_time_ms = 0.0;
    
    // Issues and warnings
    std::vector<std::string> operation_warnings;
    std::vector<std::string> operation_errors;
    std::vector<std::string> configuration_issues;
    
    std::string management_report_path;
    
    std::chrono::milliseconds getOperationDuration() const;
    std::string generateManagementReport() const;
};

} // namespace SBEnhanced

// Main enhanced Service Manager utility class
class SvcMgrEnhanced {
private:
    std::unique_ptr<SBEngineIntegration> engine;
    std::unique_ptr<jrd::Service> service_manager;
    std::atomic<bool> service_active{false};
    std::atomic<bool> shutdown_requested{false};
    
    // Service management
    std::map<uint64_t, SBEnhanced::ServiceInfo> active_services;
    std::vector<SBEnhanced::ServiceQueue> service_queues;
    std::atomic<uint64_t> next_service_id{1};
    
    // Thread management
    std::unique_ptr<std::thread> monitoring_thread;
    std::unique_ptr<std::thread> queue_processor_thread;
    std::vector<std::unique_ptr<std::thread>> worker_threads;
    std::mutex service_data_mutex;
    std::mutex queue_mutex;
    std::mutex statistics_mutex;
    
    // Configuration and state
    SBEnhanced::ServiceConfiguration current_config;
    SBEnhanced::ServiceMonitoringProgress current_progress;
    SBEnhanced::ServiceStatistics current_statistics;
    
    // Internal state
    std::vector<std::string> error_log;
    std::vector<std::string> warning_log;
    std::string last_error;

public:
    // Constructor and destructor
    SvcMgrEnhanced();
    ~SvcMgrEnhanced();
    
    // === ORIGINAL SVCMGR FUNCTIONALITY (100% Compatible) ===
    
    // Basic service operations
    bool startService(const std::string& service_parameters);
    bool stopService(uint64_t service_id);
    bool queryService(uint64_t service_id, std::string& service_status);
    
    // === ENHANCED FUNCTIONALITY ===
    
    // Service management operations
    bool startServiceManager(const SBEnhanced::ServiceConfiguration& config,
                           SBEnhanced::ServiceManagementResult& result);
    
    bool stopServiceManager(SBEnhanced::ServiceManagementResult& result);
    
    bool restartServiceManager(const SBEnhanced::ServiceConfiguration& config,
                             SBEnhanced::ServiceManagementResult& result);
    
    // Service execution and control
    uint64_t executeService(const SBEnhanced::ServiceOptions& options,
                           SBEnhanced::ServiceInfo& service_info);
    
    bool cancelService(uint64_t service_id);
    bool pauseService(uint64_t service_id);
    bool resumeService(uint64_t service_id);
    bool retryService(uint64_t service_id);
    
    // Service querying and monitoring
    bool getServiceInfo(uint64_t service_id, SBEnhanced::ServiceInfo& service_info);
    
    bool listActiveServices(std::vector<SBEnhanced::ServiceInfo>& services);
    bool listQueuedServices(std::vector<SBEnhanced::ServiceInfo>& services);
    bool listCompletedServices(std::vector<SBEnhanced::ServiceInfo>& services);
    
    bool getServicesByOperation(SBEnhanced::ServiceOperation operation,
                              std::vector<SBEnhanced::ServiceInfo>& services);
    
    bool getServicesByStatus(SBEnhanced::ServiceStatus status,
                           std::vector<SBEnhanced::ServiceInfo>& services);
    
    bool getServicesByUser(const std::string& user_name,
                         std::vector<SBEnhanced::ServiceInfo>& services);
    
    // Service queue management
    bool createServiceQueue(const std::string& queue_name,
                          SBEnhanced::ServicePriority priority,
                          uint32_t max_concurrent_services);
    
    bool deleteServiceQueue(const std::string& queue_name);
    
    bool getServiceQueues(std::vector<SBEnhanced::ServiceQueue>& queues);
    
    bool getQueueInfo(const std::string& queue_name,
                     SBEnhanced::ServiceQueue& queue_info);
    
    bool pauseQueue(const std::string& queue_name);
    bool resumeQueue(const std::string& queue_name);
    bool clearQueue(const std::string& queue_name);
    
    // Service statistics and monitoring
    bool collectServiceStatistics(SBEnhanced::ServiceStatistics& statistics);
    
    bool getServicePerformanceMetrics(std::map<std::string, double>& metrics);
    
    bool analyzeServicePerformance(const std::chrono::hours& time_window,
                                 std::vector<std::string>& analysis_results);
    
    bool identifyPerformanceBottlenecks(std::vector<std::string>& bottlenecks);
    
    bool generateServiceHealthReport(const std::string& report_format = "TEXT",
                                   const std::string& output_path = "");
    
    // Advanced service operations
    bool scheduleService(const SBEnhanced::ServiceOptions& options,
                       const std::chrono::system_clock::time_point& scheduled_time,
                       uint64_t& service_id);
    
    bool createServiceTemplate(const std::string& template_name,
                             const SBEnhanced::ServiceOptions& template_options);
    
    bool executeServiceFromTemplate(const std::string& template_name,
                                  const std::map<std::string, std::string>& parameters,
                                  uint64_t& service_id);
    
    bool bulkExecuteServices(const std::vector<SBEnhanced::ServiceOptions>& service_list,
                           std::vector<uint64_t>& service_ids);
    
    // Service monitoring and alerting
    bool enableServiceMonitoring(SBEnhanced::ServiceMonitoringLevel level);
    bool disableServiceMonitoring();
    
    bool setServiceAlert(SBEnhanced::ServiceOperation operation,
                       const std::string& alert_condition,
                       const std::function<void(const SBEnhanced::ServiceInfo&)>& alert_callback);
    
    bool removeServiceAlert(SBEnhanced::ServiceOperation operation);
    
    bool monitorServiceHealth(const std::chrono::seconds& check_interval,
                            const std::function<void(const SBEnhanced::ServiceStatistics&)>& health_callback);
    
    // Configuration management
    bool loadConfiguration(const std::string& config_file_path);
    bool saveConfiguration(const std::string& config_file_path);
    bool updateConfiguration(const SBEnhanced::ServiceConfiguration& new_config);
    SBEnhanced::ServiceConfiguration getCurrentConfiguration() const;
    
    // Service history and cleanup
    bool getServiceHistory(const std::chrono::hours& time_window,
                         std::vector<SBEnhanced::ServiceInfo>& history);
    
    bool cleanupServiceHistory(const std::chrono::hours& retention_period);
    
    bool exportServiceLogs(const std::string& export_format,
                         const std::string& output_path,
                         const std::chrono::hours& time_window);
    
    // Backup and restore services
    bool backupServiceConfiguration(const std::string& backup_path);
    bool restoreServiceConfiguration(const std::string& backup_path);
    
    // Import/Export utilities
    bool exportServiceDefinitions(const std::string& export_path,
                                const std::string& export_format = "JSON");
    
    bool importServiceDefinitions(const std::string& import_path);
    
    bool createServiceReport(const std::vector<uint64_t>& service_ids,
                           const std::string& report_format,
                           const std::string& output_path);
    
    // Progress monitoring and control
    SBEnhanced::ServiceMonitoringProgress getCurrentProgress() const;
    bool isServiceManagerActive() const;
    void requestShutdown();
    
    // Error handling and logging
    std::vector<std::string> getErrors() const;
    std::vector<std::string> getWarnings() const;
    std::string getLastError() const;
    void clearErrorLog();
    
    // Configuration and validation
    bool validateServiceConfiguration(const SBEnhanced::ServiceConfiguration& config);
    bool testServiceConnection(const SBEnhanced::ServiceConfiguration& config);

private:
    // Internal initialization
    bool initializeEngine();
    bool initializeServiceManager();
    
    // Service execution management
    void serviceExecutionLoop();
    void queueProcessorLoop();
    void monitoringLoop();
    
    // Core service operations
    bool executeServiceInternal(const SBEnhanced::ServiceOptions& options,
                              SBEnhanced::ServiceInfo& service_info);
    
    bool processServiceQueue(SBEnhanced::ServiceQueue& queue);
    
    bool updateServiceProgress(uint64_t service_id,
                             double progress_percentage,
                             const std::string& current_operation);
    
    // Service state management
    bool addServiceToQueue(const SBEnhanced::ServiceOptions& options,
                         const std::string& queue_name,
                         uint64_t& service_id);
    
    bool removeServiceFromQueue(uint64_t service_id, const std::string& queue_name);
    
    bool moveServiceToCompleted(uint64_t service_id);
    
    // Statistics and monitoring
    bool updateServiceStatistics();
    bool analyzeServiceTrends();
    bool generatePerformanceAlerts();
    
    // Service queue algorithms
    SBEnhanced::ServiceQueue* findBestQueue(SBEnhanced::ServicePriority priority);
    bool balanceServiceQueues();
    bool optimizeQueuePerformance();
    
    // Progress tracking helpers
    void updateProgress(const std::string& operation, const std::string& scope);
    void logError(const std::string& operation, const std::string& error);
    void logWarning(const std::string& operation, const std::string& warning);
    void logInfo(const std::string& operation, const std::string& info);
    
    // Configuration helpers
    bool validateConfigurationInternal(const SBEnhanced::ServiceConfiguration& config,
                                     std::vector<std::string>& issues);
    
    bool applyConfiguration(const SBEnhanced::ServiceConfiguration& config);
    
    // Service manager helpers
    bool startServiceManagerInternal();
    bool stopServiceManagerInternal();
    void cleanupResources();
    
    // Database connection helpers
    bool establishServiceConnection(const SBEnhanced::ServiceConfiguration& config,
                                  std::unique_ptr<jrd::Service>& service);
    
    void closeServiceConnection(std::unique_ptr<jrd::Service>& service);
    
    // Service execution helpers
    bool prepareServiceExecution(const SBEnhanced::ServiceOptions& options,
                               SBEnhanced::ServiceInfo& service_info);
    
    bool finalizeServiceExecution(SBEnhanced::ServiceInfo& service_info,
                                bool success);
    
    bool handleServiceError(SBEnhanced::ServiceInfo& service_info,
                          const std::string& error_message);
};

// Utility functions for enhanced Service Manager
namespace SBEnhanced {

// Quick service operations
bool quickExecuteService(const std::string& service_operation,
                        const std::vector<std::string>& parameters);

bool quickQueryService(uint64_t service_id, std::string& status);

bool quickCancelService(uint64_t service_id);

// Service information helpers
std::string formatServiceDuration(const std::chrono::milliseconds& duration);
std::string formatServiceSize(uint64_t size_bytes);
double calculateServiceEfficiency(const ServiceStatistics& statistics);

// Service queue utilities
bool isServiceQueueHealthy(const ServiceQueue& queue);
uint32_t calculateOptimalWorkerCount(const ServiceQueue& queue);
ServicePriority determineServicePriority(ServiceOperation operation);

// Performance analysis utilities
double calculateThroughput(const ServiceStatistics& statistics);
std::vector<std::string> identifyServiceBottlenecks(const ServiceStatistics& statistics);
bool isServicePerformanceOptimal(const ServiceStatistics& statistics);

// Export utilities
bool exportToCSV(const std::vector<ServiceInfo>& services, const std::string& filename);
bool exportToJSON(const std::vector<ServiceInfo>& services, const std::string& filename);
bool exportToXML(const std::vector<ServiceInfo>& services, const std::string& filename);

// Compatibility helpers for command-line usage
int parseSvcMgrCommandLine(int argc, char* argv[],
                          ServiceConfiguration& config,
                          ServiceOptions& service_opts);

bool executeClassicSvcMgrCommand(const std::string& command_line);

} // namespace SBEnhanced
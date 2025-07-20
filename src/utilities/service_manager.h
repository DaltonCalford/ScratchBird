#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <condition_variable>
#include <queue>
#include <future>

// Forward declarations
extern "C" {
    #include <ibase.h>
}

namespace SBEnhanced {
    
    // Service Types
    enum class ServiceType {
        BACKUP,
        RESTORE,
        REPAIR,
        STATISTICS,
        VALIDATION,
        SWEEP,
        MAINTENANCE,
        MONITORING,
        CUSTOM
    };
    
    // Service Status
    enum class ServiceStatus {
        PENDING,
        RUNNING,
        PAUSED,
        COMPLETED,
        FAILED,
        CANCELLED,
        TIMEOUT
    };
    
    // Service Priority
    enum class ServicePriority {
        LOW,
        NORMAL,
        HIGH,
        CRITICAL
    };
    
    // Service Configuration
    struct ServiceConfig {
        ServiceType type = ServiceType::CUSTOM;
        ServicePriority priority = ServicePriority::NORMAL;
        std::string name;
        std::string description;
        std::chrono::seconds timeout{3600}; // 1 hour default
        std::chrono::seconds retry_interval{60};
        uint32_t max_retries = 3;
        bool auto_retry = true;
        bool run_in_background = true;
        bool persistent = false;
        std::map<std::string, std::string> parameters;
        std::function<void(const std::string&)> progress_callback;
        std::function<void(const std::string&, const std::string&)> completion_callback;
        std::function<void(const std::string&, const std::string&)> error_callback;
    };
    
    // Service Info
    struct ServiceInfo {
        std::string service_id;
        std::string name;
        std::string description;
        ServiceType type;
        ServiceStatus status;
        ServicePriority priority;
        
        // Service handle
        isc_svc_handle svc_handle = 0;
        
        // Timing
        std::chrono::steady_clock::time_point created_time;
        std::chrono::steady_clock::time_point started_time;
        std::chrono::steady_clock::time_point completed_time;
        std::chrono::steady_clock::time_point last_activity_time;
        std::atomic<std::chrono::microseconds> total_runtime{std::chrono::microseconds::zero()};
        
        // Configuration
        ServiceConfig config;
        
        // Progress tracking
        std::atomic<double> progress_percentage{0.0};
        std::atomic<uint64_t> items_processed{0};
        std::atomic<uint64_t> items_total{0};
        std::atomic<uint64_t> bytes_processed{0};
        std::atomic<uint64_t> bytes_total{0};
        std::string current_operation;
        std::string current_item;
        
        // Statistics
        std::atomic<uint64_t> operations_completed{0};
        std::atomic<uint64_t> operations_failed{0};
        std::atomic<uint64_t> retry_count{0};
        std::atomic<uint64_t> warning_count{0};
        std::atomic<uint64_t> error_count{0};
        
        // Results
        std::vector<std::string> output_log;
        std::vector<std::string> error_log;
        std::vector<std::string> warning_log;
        std::string result_data;
        std::map<std::string, std::string> result_metadata;
        
        // Thread information
        std::thread::id worker_thread;
        std::atomic<bool> cancellation_requested{false};
        std::atomic<bool> pause_requested{false};
        
        // Dependencies
        std::vector<std::string> depends_on;
        std::vector<std::string> dependents;
        
        // Resource usage
        std::atomic<uint64_t> memory_usage{0};
        std::atomic<uint64_t> cpu_time{0};
        std::atomic<uint64_t> io_reads{0};
        std::atomic<uint64_t> io_writes{0};
        
        // Callbacks
        std::function<void(const ServiceInfo&)> on_start;
        std::function<void(const ServiceInfo&)> on_progress;
        std::function<void(const ServiceInfo&)> on_complete;
        std::function<void(const ServiceInfo&)> on_error;
        std::function<void(const ServiceInfo&)> on_cancel;
    };
    
    // Service Manager Statistics
    struct ServiceManagerStats {
        std::atomic<uint64_t> total_services{0};
        std::atomic<uint64_t> pending_services{0};
        std::atomic<uint64_t> running_services{0};
        std::atomic<uint64_t> completed_services{0};
        std::atomic<uint64_t> failed_services{0};
        std::atomic<uint64_t> cancelled_services{0};
        std::atomic<uint64_t> timeout_services{0};
        std::atomic<uint64_t> retry_services{0};
        std::atomic<uint32_t> active_workers{0};
        std::atomic<uint32_t> idle_workers{0};
        std::atomic<uint32_t> max_workers{0};
        std::atomic<std::chrono::microseconds> average_service_time{std::chrono::microseconds::zero()};
        std::atomic<std::chrono::microseconds> total_processing_time{std::chrono::microseconds::zero()};
        std::atomic<uint64_t> peak_memory_usage{0};
        std::atomic<uint64_t> current_memory_usage{0};
        std::atomic<uint64_t> total_io_operations{0};
        std::atomic<uint64_t> service_queue_size{0};
        std::atomic<uint64_t> service_queue_max_size{0};
    };
    
    // Background Service Task
    struct BackgroundTask {
        std::string task_id;
        std::string name;
        std::function<bool()> task_function;
        std::chrono::steady_clock::time_point next_run_time;
        std::chrono::seconds interval;
        bool is_recurring = false;
        bool is_enabled = true;
        std::atomic<bool> is_running{false};
        std::atomic<uint64_t> execution_count{0};
        std::atomic<uint64_t> success_count{0};
        std::atomic<uint64_t> failure_count{0};
        std::chrono::steady_clock::time_point last_run_time;
        std::chrono::steady_clock::time_point last_success_time;
        std::chrono::steady_clock::time_point last_failure_time;
        std::atomic<std::chrono::microseconds> average_execution_time{std::chrono::microseconds::zero()};
    };
    
} // namespace SBEnhanced

// Service Manager Class
class ServiceManager {
private:
    // Service registry
    std::map<std::string, std::unique_ptr<SBEnhanced::ServiceInfo>> services;
    std::mutex services_mutex;
    
    // Service queue
    std::priority_queue<std::string> service_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_condition;
    
    // Worker threads
    std::vector<std::thread> worker_threads;
    std::atomic<bool> workers_running{false};
    std::atomic<uint32_t> active_workers{0};
    uint32_t max_workers = 4;
    
    // Background tasks
    std::map<std::string, std::unique_ptr<SBEnhanced::BackgroundTask>> background_tasks;
    std::thread background_thread;
    std::atomic<bool> background_running{false};
    std::mutex background_mutex;
    std::condition_variable background_condition;
    
    // Statistics
    mutable SBEnhanced::ServiceManagerStats stats;
    
    // Configuration
    std::string server_name;
    std::string service_username;
    std::string service_password;
    std::chrono::seconds default_timeout{3600};
    bool auto_start_services = true;
    
    // Error handling
    mutable std::vector<std::string> error_log;
    mutable std::mutex error_mutex;
    std::atomic<uint64_t> error_count{0};
    
    // Performance monitoring
    std::atomic<bool> performance_monitoring_enabled{false};
    std::map<std::string, std::atomic<uint64_t>> performance_counters;
    mutable std::mutex performance_mutex;
    
public:
    ServiceManager();
    ~ServiceManager();
    
    // Initialization
    bool initialize(const std::string& server_name, const std::string& username,
                   const std::string& password, uint32_t max_workers = 4);
    bool shutdown();
    
    // Service lifecycle
    std::string createService(const SBEnhanced::ServiceConfig& config);
    bool startService(const std::string& service_id);
    bool pauseService(const std::string& service_id);
    bool resumeService(const std::string& service_id);
    bool cancelService(const std::string& service_id);
    bool removeService(const std::string& service_id);
    
    // Service queries
    bool serviceExists(const std::string& service_id) const;
    SBEnhanced::ServiceStatus getServiceStatus(const std::string& service_id) const;
    std::shared_ptr<SBEnhanced::ServiceInfo> getServiceInfo(const std::string& service_id) const;
    std::vector<std::string> getActiveServices() const;
    std::vector<std::string> getCompletedServices() const;
    std::vector<std::string> getFailedServices() const;
    std::vector<std::string> getAllServices() const;
    
    // Service monitoring
    bool waitForService(const std::string& service_id, std::chrono::seconds timeout = std::chrono::seconds{0});
    bool waitForAllServices(std::chrono::seconds timeout = std::chrono::seconds{0});
    double getServiceProgress(const std::string& service_id) const;
    std::string getServiceOutput(const std::string& service_id) const;
    std::vector<std::string> getServiceErrors(const std::string& service_id) const;
    
    // Specialized services
    std::string createBackupService(const std::string& database_path, const std::string& backup_path,
                                   const std::map<std::string, std::string>& options = {});
    std::string createRestoreService(const std::string& backup_path, const std::string& database_path,
                                    const std::map<std::string, std::string>& options = {});
    std::string createRepairService(const std::string& database_path,
                                   const std::map<std::string, std::string>& options = {});
    std::string createStatisticsService(const std::string& database_path,
                                       const std::map<std::string, std::string>& options = {});
    std::string createValidationService(const std::string& database_path,
                                       const std::map<std::string, std::string>& options = {});
    std::string createSweepService(const std::string& database_path,
                                  const std::map<std::string, std::string>& options = {});
    
    // Background task management
    std::string scheduleBackgroundTask(const std::string& name, std::function<bool()> task,
                                      std::chrono::seconds interval, bool recurring = true);
    bool cancelBackgroundTask(const std::string& task_id);
    bool pauseBackgroundTask(const std::string& task_id);
    bool resumeBackgroundTask(const std::string& task_id);
    std::vector<std::string> getBackgroundTasks() const;
    std::shared_ptr<SBEnhanced::BackgroundTask> getBackgroundTaskInfo(const std::string& task_id) const;
    
    // Worker management
    bool setMaxWorkers(uint32_t max_workers);
    uint32_t getMaxWorkers() const;
    uint32_t getActiveWorkers() const;
    uint32_t getIdleWorkers() const;
    bool addWorker();
    bool removeWorker();
    
    // Service dependencies
    bool addServiceDependency(const std::string& service_id, const std::string& depends_on_service_id);
    bool removeServiceDependency(const std::string& service_id, const std::string& depends_on_service_id);
    std::vector<std::string> getServiceDependencies(const std::string& service_id) const;
    std::vector<std::string> getServiceDependents(const std::string& service_id) const;
    
    // Statistics
    SBEnhanced::ServiceManagerStats getStatistics() const;
    bool resetStatistics();
    
    // Configuration
    bool setDefaultTimeout(std::chrono::seconds timeout);
    std::chrono::seconds getDefaultTimeout() const;
    bool setAutoStartServices(bool auto_start);
    bool getAutoStartServices() const;
    
    // Error handling
    std::vector<std::string> getErrorLog() const;
    void clearErrorLog();
    uint64_t getErrorCount() const;
    std::string getLastError() const;
    
    // Performance monitoring
    bool enablePerformanceMonitoring(bool enable = true);
    std::map<std::string, uint64_t> getPerformanceCounters() const;
    bool resetPerformanceCounters();
    
    // Utility methods
    bool isInitialized() const;
    std::string generateServiceId() const;
    std::string generateTaskId() const;
    
private:
    // Internal service management
    bool executeService(SBEnhanced::ServiceInfo* service);
    bool executeBackupService(SBEnhanced::ServiceInfo* service);
    bool executeRestoreService(SBEnhanced::ServiceInfo* service);
    bool executeRepairService(SBEnhanced::ServiceInfo* service);
    bool executeStatisticsService(SBEnhanced::ServiceInfo* service);
    bool executeValidationService(SBEnhanced::ServiceInfo* service);
    bool executeSweepService(SBEnhanced::ServiceInfo* service);
    bool executeCustomService(SBEnhanced::ServiceInfo* service);
    
    // Worker thread management
    void workerLoop();
    void backgroundTaskLoop();
    
    // Service dependency resolution
    bool canStartService(const std::string& service_id) const;
    std::vector<std::string> getServiceExecutionOrder(const std::vector<std::string>& service_ids) const;
    bool checkCircularDependencies(const std::string& service_id) const;
    
    // Service Parameter Block (SPB) building
    std::string buildSPB(const std::map<std::string, std::string>& parameters);
    
    // Service output processing
    bool processServiceOutput(SBEnhanced::ServiceInfo* service);
    bool parseServiceResult(SBEnhanced::ServiceInfo* service, const std::string& output);
    
    // Error handling helpers
    void logError(const std::string& operation, const std::string& error) const;
    void logPerformance(const std::string& operation, std::chrono::microseconds duration) const;
    std::string formatISCError(const ISC_STATUS* status_vector) const;
    
    // Statistics helpers
    void updateStats(const std::string& operation, std::chrono::microseconds duration) const;
    void incrementCounter(const std::string& counter_name) const;
    
    // Utility helpers
    bool isServiceExpired(const SBEnhanced::ServiceInfo* service) const;
    std::string formatServiceInfo(const SBEnhanced::ServiceInfo* service) const;
    SBEnhanced::ServicePriority stringToPriority(const std::string& priority) const;
    std::string priorityToString(SBEnhanced::ServicePriority priority) const;
};
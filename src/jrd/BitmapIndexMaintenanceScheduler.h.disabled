/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		BitmapIndexMaintenanceScheduler.h
 *	DESCRIPTION:	Automatic bitmap index maintenance scheduler
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * 2025.07.23 - ScratchBird Bitmap Index Maintenance Scheduler
 */

#ifndef JRD_BITMAP_INDEX_MAINTENANCE_SCHEDULER_H
#define JRD_BITMAP_INDEX_MAINTENANCE_SCHEDULER_H

#include "../jrd/constants.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "BitmapIndex.h"
#include <vector>
#include <memory>
#include <map>
#include <queue>

namespace Jrd {

// Forward declarations
class thread_db;
class MemoryPool;
class Database;
struct index_desc;
class CompressedBitmap;
class BitmapIndex;

//----------------------------
// Maintenance Task Types
//----------------------------

enum MaintenanceTaskType : UCHAR
{
    MAINTENANCE_COMPRESSION = 0,        // Bitmap compression optimization
    MAINTENANCE_DEFRAGMENTATION = 1,    // Index defragmentation
    MAINTENANCE_STATISTICS_UPDATE = 2,  // Statistics refresh
    MAINTENANCE_REBUILD = 3,            // Full index rebuild
    MAINTENANCE_VALIDATION = 4,         // Consistency validation
    MAINTENANCE_CLEANUP = 5,            // Cleanup deleted entries
    MAINTENANCE_REBALANCING = 6,        // Rebalance index structure
    MAINTENANCE_ARCHIVAL = 7,           // Archive old data
    MAINTENANCE_BACKUP = 8,             // Create index backup
    MAINTENANCE_ANALYSIS = 9            // Performance analysis
};

//----------------------------
// Maintenance Priority Levels
//----------------------------

enum MaintenancePriority : UCHAR
{
    PRIORITY_CRITICAL = 0,              // Critical maintenance (immediate)
    PRIORITY_HIGH = 1,                  // High priority (within hours)
    PRIORITY_MEDIUM = 2,                // Medium priority (within day)
    PRIORITY_LOW = 3,                   // Low priority (within week)
    PRIORITY_BACKGROUND = 4             // Background (when system idle)
};

//----------------------------
// Maintenance Trigger Conditions
//----------------------------

enum MaintenanceTrigger : UCHAR
{
    TRIGGER_TIME_BASED = 0,             // Scheduled at specific times
    TRIGGER_THRESHOLD_BASED = 1,        // Triggered by thresholds
    TRIGGER_EVENT_BASED = 2,            // Triggered by events
    TRIGGER_ADAPTIVE = 3,               // Adaptive triggering
    TRIGGER_MANUAL = 4,                 // Manual triggering
    TRIGGER_SYSTEM_IDLE = 5             // Triggered during idle periods
};

//----------------------------
// Index Health Assessment
//----------------------------

struct IndexHealthMetrics
{
    // Compression efficiency
    double compression_ratio;           // Current compression ratio
    double optimal_compression_ratio;   // Achievable compression ratio
    ULONG uncompressed_size;            // Uncompressed bitmap size
    ULONG compressed_size;              // Current compressed size
    
    // Fragmentation analysis
    double fragmentation_level;         // Index fragmentation (0.0-1.0)
    ULONG total_pages;                  // Total index pages
    ULONG used_pages;                   // Actually used pages
    ULONG free_space;                   // Available free space
    
    // Performance metrics
    double query_performance_ratio;     // Current vs optimal performance
    ULONG average_scan_time_ms;         // Average scan time
    ULONG cache_hit_ratio;              // Cache effectiveness
    ULONG disk_io_operations;           // I/O operations per query
    
    // Update patterns
    ULONG updates_since_maintenance;    // Updates since last maintenance
    double update_frequency;            // Updates per hour
    ULONG hot_spot_count;               // Number of frequently updated regions
    double update_distribution;         // How evenly updates are distributed
    
    // Statistics accuracy
    GDS_TIMESTAMP last_statistics_update; // When statistics were last updated
    double statistics_accuracy;         // Estimated accuracy of statistics
    bool statistics_outdated;           // True if statistics need refresh
    
    // Overall health score
    double health_score;                // Overall health (0.0-1.0, 1.0 = perfect)
    bool needs_immediate_attention;     // True if critical issues found
    
    IndexHealthMetrics()
        : compression_ratio(1.0), optimal_compression_ratio(1.0),
          uncompressed_size(0), compressed_size(0), fragmentation_level(0.0),
          total_pages(0), used_pages(0), free_space(0),
          query_performance_ratio(1.0), average_scan_time_ms(0),
          cache_hit_ratio(100), disk_io_operations(0),
          updates_since_maintenance(0), update_frequency(0.0),
          hot_spot_count(0), update_distribution(1.0),
          last_statistics_update(0), statistics_accuracy(1.0),
          statistics_outdated(false), health_score(1.0),
          needs_immediate_attention(false)
    {
    }
    
    // Health assessment helpers
    bool needsCompression() const {
        return compression_ratio < (optimal_compression_ratio * 0.8);
    }
    
    bool needsDefragmentation() const {
        return fragmentation_level > 0.3; // 30% fragmentation threshold
    }
    
    bool needsStatisticsUpdate() const {
        return statistics_outdated || statistics_accuracy < 0.8;
    }
    
    bool needsRebuild() const {
        return health_score < 0.4 || fragmentation_level > 0.7;
    }
};

//----------------------------
// Maintenance Task Definition
//----------------------------

struct MaintenanceTask
{
    ULONG task_id;                      // Unique task identifier
    MaintenanceTaskType task_type;      // Type of maintenance task
    MaintenancePriority priority;       // Task priority level
    MaintenanceTrigger trigger_type;    // What triggered this task
    
    // Target information
    Database* target_database;          // Database containing the index
    USHORT index_id;                    // Target index identifier
    const index_desc* index_descriptor; // Index descriptor
    
    // Scheduling information
    GDS_TIMESTAMP creation_time;        // When task was created
    GDS_TIMESTAMP scheduled_time;       // When task should execute
    GDS_TIMESTAMP deadline;             // Task deadline (for critical tasks)
    ULONG estimated_duration_ms;       // Estimated execution time
    
    // Execution constraints
    bool requires_exclusive_access;     // True if needs exclusive index access
    bool can_run_online;                // True if can run with concurrent access
    ULONG max_memory_usage_kb;          // Maximum memory usage allowed
    ULONG max_cpu_percentage;           // Maximum CPU utilization allowed
    
    // Task parameters
    std::map<ScratchBird::string, ScratchBird::string> parameters; // Task-specific parameters
    
    // Execution tracking
    enum TaskStatus : UCHAR {
        STATUS_PENDING = 0,             // Task is waiting to execute
        STATUS_RUNNING = 1,             // Task is currently executing
        STATUS_COMPLETED = 2,           // Task completed successfully
        STATUS_FAILED = 3,              // Task failed
        STATUS_CANCELLED = 4,           // Task was cancelled
        STATUS_DEFERRED = 5             // Task was deferred to later
    };
    
    TaskStatus status;                  // Current task status
    GDS_TIMESTAMP start_time;           // When task started executing
    GDS_TIMESTAMP completion_time;      // When task completed
    ULONG actual_duration_ms;           // Actual execution time
    ScratchBird::string error_message;  // Error message if failed
    
    // Dependencies and prerequisites
    std::vector<ULONG> prerequisite_tasks; // Tasks that must complete first
    std::vector<ULONG> dependent_tasks;    // Tasks that depend on this one
    
    MaintenanceTask()
        : task_id(0), task_type(MAINTENANCE_COMPRESSION), priority(PRIORITY_MEDIUM),
          trigger_type(TRIGGER_THRESHOLD_BASED), target_database(nullptr),
          index_id(0), index_descriptor(nullptr), creation_time(0),
          scheduled_time(0), deadline(0), estimated_duration_ms(0),
          requires_exclusive_access(false), can_run_online(true),
          max_memory_usage_kb(0), max_cpu_percentage(50),
          status(STATUS_PENDING), start_time(0), completion_time(0),
          actual_duration_ms(0)
    {
    }
    
    // Task comparison for priority queue
    bool operator<(const MaintenanceTask& other) const {
        if (priority != other.priority) {
            return priority > other.priority; // Higher priority first
        }
        return scheduled_time > other.scheduled_time; // Earlier time first
    }
};

//----------------------------
// Maintenance Schedule Configuration
//----------------------------

struct MaintenanceScheduleConfig
{
    // Scheduling windows
    struct TimeWindow {
        UCHAR start_hour;               // Start hour (0-23)
        UCHAR start_minute;             // Start minute (0-59)
        UCHAR end_hour;                 // End hour (0-23)
        UCHAR end_minute;               // End minute (0-59)
        UCHAR day_of_week;              // Day of week (0=Sunday, 6=Saturday, 7=Any)
        
        TimeWindow() : start_hour(0), start_minute(0), end_hour(23), end_minute(59), day_of_week(7) {}
        
        bool isInWindow(GDS_TIMESTAMP timestamp) const;
    };
    
    std::vector<TimeWindow> maintenance_windows;    // Allowed maintenance windows
    std::vector<TimeWindow> blackout_windows;       // Forbidden maintenance windows
    
    // Resource constraints
    ULONG max_concurrent_tasks;        // Maximum concurrent maintenance tasks
    ULONG max_memory_usage_mb;         // Maximum total memory for maintenance
    ULONG max_cpu_percentage;          // Maximum CPU usage for maintenance
    ULONG max_io_bandwidth_mbps;       // Maximum I/O bandwidth for maintenance
    
    // Trigger thresholds
    double compression_threshold;       // Trigger compression when ratio drops below
    double fragmentation_threshold;     // Trigger defrag when fragmentation exceeds
    ULONG update_count_threshold;       // Trigger after this many updates
    ULONG statistics_age_hours;         // Refresh statistics after this time
    double performance_degradation_threshold; // Trigger when performance drops
    
    // Scheduling preferences
    bool prefer_off_hours;              // Prefer running during off-hours
    bool adaptive_scheduling;           // Adapt schedule based on system load
    ULONG task_retry_count;             // Number of retries for failed tasks
    ULONG task_timeout_hours;           // Timeout for long-running tasks
    
    MaintenanceScheduleConfig()
        : max_concurrent_tasks(2), max_memory_usage_mb(512),
          max_cpu_percentage(25), max_io_bandwidth_mbps(100),
          compression_threshold(0.7), fragmentation_threshold(0.3),
          update_count_threshold(10000), statistics_age_hours(24),
          performance_degradation_threshold(0.8), prefer_off_hours(true),
          adaptive_scheduling(true), task_retry_count(3), task_timeout_hours(4)
    {
        // Default maintenance window: 2 AM - 6 AM daily
        TimeWindow default_window;
        default_window.start_hour = 2;
        default_window.end_hour = 6;
        maintenance_windows.push_back(default_window);
    }
};

//----------------------------
// System Load Monitor
//----------------------------

class SystemLoadMonitor
{
public:
    explicit SystemLoadMonitor(MemoryPool* pool);
    ~SystemLoadMonitor();
    
    // Load monitoring
    struct SystemLoad {
        double cpu_utilization;         // CPU utilization percentage (0-100)
        ULONG memory_usage_mb;          // Memory usage in MB
        ULONG disk_io_operations;       // Disk I/O operations per second
        double disk_utilization;        // Disk utilization percentage
        ULONG active_connections;       // Number of active database connections
        ULONG active_transactions;      // Number of active transactions
        double query_throughput;        // Queries per second
        
        SystemLoad()
            : cpu_utilization(0.0), memory_usage_mb(0), disk_io_operations(0),
              disk_utilization(0.0), active_connections(0), active_transactions(0),
              query_throughput(0.0)
        {
        }
        
        bool isSystemIdle() const {
            return cpu_utilization < 20.0 && disk_utilization < 30.0 && 
                   active_connections < 5 && query_throughput < 10.0;
        }
        
        bool isSystemBusy() const {
            return cpu_utilization > 80.0 || disk_utilization > 80.0 || 
                   active_connections > 50 || query_throughput > 100.0;
        }
    };
    
    void startMonitoring();
    void stopMonitoring();
    
    SystemLoad getCurrentLoad() const;
    SystemLoad getAverageLoad(ULONG minutes) const;
    bool isSystemIdle() const;
    bool isSystemBusy() const;
    
    // Load prediction
    SystemLoad predictLoadInMinutes(ULONG minutes) const;
    bool willSystemBeIdleInMinutes(ULONG minutes) const;

private:
    MemoryPool* m_pool;
    bool m_monitoring_active;
    
    // Load history
    struct LoadSample {
        GDS_TIMESTAMP timestamp;
        SystemLoad load;
        
        LoadSample() : timestamp(0) {}
    };
    
    std::vector<LoadSample> m_load_history;
    mutable ScratchBird::Mutex m_history_mutex;
    
    static constexpr ULONG MAX_HISTORY_SAMPLES = 1440; // 24 hours of minute samples
    
    // Monitoring thread
    void monitoringThreadProc();
    SystemLoad collectCurrentLoad();
    void recordLoadSample(const SystemLoad& load);
    void cleanupOldSamples();
};

//----------------------------
// Bitmap Index Maintenance Scheduler
//----------------------------

/**
 * Main scheduler for automatic bitmap index maintenance
 */
class BitmapIndexMaintenanceScheduler
{
public:
    explicit BitmapIndexMaintenanceScheduler(MemoryPool* pool);
    ~BitmapIndexMaintenanceScheduler();

    // Scheduler lifecycle
    bool initialize(Database* database);
    void shutdown();
    bool isRunning() const;
    
    // Task scheduling
    ULONG scheduleMaintenanceTask(const MaintenanceTask& task);
    bool cancelMaintenanceTask(ULONG task_id);
    bool deferMaintenanceTask(ULONG task_id, GDS_TIMESTAMP new_time);
    
    // Automatic scheduling
    void analyzeAndScheduleMaintenance(thread_db* tdbb);
    void scheduleRoutineMaintenance(thread_db* tdbb);
    void scheduleEmergencyMaintenance(thread_db* tdbb, const index_desc* idx, MaintenanceTaskType task_type);
    
    // Index health monitoring
    IndexHealthMetrics assessIndexHealth(thread_db* tdbb, const index_desc* idx) const;
    std::vector<const index_desc*> identifyMaintenanceNeeds(thread_db* tdbb) const;
    
    // Task execution
    void processScheduledTasks(thread_db* tdbb);
    bool executeTask(thread_db* tdbb, MaintenanceTask& task);
    
    // Configuration
    void setScheduleConfig(const MaintenanceScheduleConfig& config);
    MaintenanceScheduleConfig getScheduleConfig() const;
    
    void enableMaintenanceType(MaintenanceTaskType task_type, bool enabled = true);
    bool isMaintenanceTypeEnabled(MaintenanceTaskType task_type) const;
    
    // Statistics and monitoring
    struct SchedulerStatistics {
        ULONG total_tasks_scheduled;    // Total tasks scheduled
        ULONG tasks_completed;          // Successfully completed tasks
        ULONG tasks_failed;             // Failed tasks
        ULONG tasks_cancelled;          // Cancelled tasks
        ULONG tasks_currently_running;  // Currently executing tasks
        double average_execution_time;  // Average task execution time
        double scheduler_efficiency;    // Scheduler efficiency rating
        GDS_TIMESTAMP last_analysis;    // Last maintenance analysis time
        
        SchedulerStatistics()
            : total_tasks_scheduled(0), tasks_completed(0), tasks_failed(0),
              tasks_cancelled(0), tasks_currently_running(0),
              average_execution_time(0.0), scheduler_efficiency(1.0),
              last_analysis(0)
        {
        }
    };
    
    SchedulerStatistics getSchedulerStatistics() const;
    void resetStatistics();
    
    // Task management
    std::vector<MaintenanceTask> getPendingTasks() const;
    std::vector<MaintenanceTask> getRunningTasks() const;
    std::vector<MaintenanceTask> getCompletedTasks(GDS_TIMESTAMP since) const;
    
    MaintenanceTask* findTask(ULONG task_id);
    const MaintenanceTask* findTask(ULONG task_id) const;
    
    // System integration
    void onDatabaseAttach(Database* database);
    void onDatabaseDetach(Database* database);
    void onIndexCreate(thread_db* tdbb, const index_desc* idx);
    void onIndexDrop(thread_db* tdbb, const index_desc* idx);
    void onIndexUpdate(thread_db* tdbb, const index_desc* idx, ULONG update_count);

private:
    MemoryPool* m_pool;
    Database* m_database;
    bool m_is_running;
    
    // Task management
    std::priority_queue<MaintenanceTask> m_task_queue;
    std::map<ULONG, MaintenanceTask> m_running_tasks;
    std::vector<MaintenanceTask> m_completed_tasks;
    ULONG m_next_task_id;
    mutable ScratchBird::Mutex m_tasks_mutex;
    
    // Configuration and monitoring
    MaintenanceScheduleConfig m_config;
    std::map<MaintenanceTaskType, bool> m_enabled_task_types;
    std::unique_ptr<SystemLoadMonitor> m_load_monitor;
    
    // Statistics
    mutable SchedulerStatistics m_statistics;
    mutable ScratchBird::Mutex m_statistics_mutex;
    
    // Scheduling thread
    void schedulingThreadProc();
    bool m_scheduler_active;
    
    // Task creation helpers
    MaintenanceTask createCompressionTask(thread_db* tdbb, const index_desc* idx, const IndexHealthMetrics& health);
    MaintenanceTask createDefragmentationTask(thread_db* tdbb, const index_desc* idx, const IndexHealthMetrics& health);
    MaintenanceTask createStatisticsUpdateTask(thread_db* tdbb, const index_desc* idx, const IndexHealthMetrics& health);
    MaintenanceTask createRebuildTask(thread_db* tdbb, const index_desc* idx, const IndexHealthMetrics& health);
    MaintenanceTask createValidationTask(thread_db* tdbb, const index_desc* idx, const IndexHealthMetrics& health);
    MaintenanceTask createCleanupTask(thread_db* tdbb, const index_desc* idx, const IndexHealthMetrics& health);
    
    // Task execution implementations
    bool executeCompressionTask(thread_db* tdbb, MaintenanceTask& task);
    bool executeDefragmentationTask(thread_db* tdbb, MaintenanceTask& task);
    bool executeStatisticsUpdateTask(thread_db* tdbb, MaintenanceTask& task);
    bool executeRebuildTask(thread_db* tdbb, MaintenanceTask& task);
    bool executeValidationTask(thread_db* tdbb, MaintenanceTask& task);
    bool executeCleanupTask(thread_db* tdbb, MaintenanceTask& task);
    
    // Health assessment helpers
    double assessCompressionHealth(thread_db* tdbb, const index_desc* idx) const;
    double assessFragmentationHealth(thread_db* tdbb, const index_desc* idx) const;
    double assessPerformanceHealth(thread_db* tdbb, const index_desc* idx) const;
    double assessStatisticsHealth(thread_db* tdbb, const index_desc* idx) const;
    
    // Scheduling logic
    bool shouldScheduleTask(const MaintenanceTask& task) const;
    GDS_TIMESTAMP calculateOptimalScheduleTime(const MaintenanceTask& task) const;
    MaintenancePriority calculateTaskPriority(const IndexHealthMetrics& health, MaintenanceTaskType task_type) const;
    
    // Resource management
    bool hasAvailableResources(const MaintenanceTask& task) const;
    void reserveResources(const MaintenanceTask& task);
    void releaseResources(const MaintenanceTask& task);
    
    // Adaptive scheduling
    void updateSchedulingModel(const MaintenanceTask& completed_task);
    double predictTaskExecutionTime(const MaintenanceTask& task) const;
    void adaptToSystemLoad();
    
    // Task dependencies
    bool arePrerequisitesMet(const MaintenanceTask& task) const;
    void updateDependentTasks(ULONG completed_task_id);
    
    // Utility methods
    void updateStatistics(const MaintenanceTask& task);
    void logTaskExecution(const MaintenanceTask& task, const ScratchBird::string& message);
    void notifyTaskCompletion(const MaintenanceTask& task);
    
    // Task queue management
    void addTaskToQueue(const MaintenanceTask& task);
    MaintenanceTask* getNextReadyTask();
    void removeCompletedTasks();
    void retryFailedTasks();
};

//----------------------------
// Global Maintenance Manager
//----------------------------

/**
 * Global manager for bitmap index maintenance across all databases
 */
class GlobalBitmapMaintenanceManager
{
public:
    static GlobalBitmapMaintenanceManager* getInstance();
    
    // Global management
    void registerDatabase(Database* database);
    void unregisterDatabase(Database* database);
    
    BitmapIndexMaintenanceScheduler* getSchedulerForDatabase(Database* database);
    
    // Global operations
    void performGlobalMaintenance();
    void scheduleGlobalMaintenanceWindow(GDS_TIMESTAMP start_time, ULONG duration_minutes);
    
    // Global configuration
    void setGlobalMaintenanceConfig(const MaintenanceScheduleConfig& config);
    MaintenanceScheduleConfig getGlobalMaintenanceConfig() const;
    
    // Global statistics
    struct GlobalMaintenanceStatistics {
        ULONG total_databases;          // Number of registered databases
        ULONG total_indexes;            // Total bitmap indexes being maintained
        ULONG active_maintenance_tasks; // Currently running maintenance tasks
        ULONG completed_tasks_today;    // Tasks completed today
        double global_health_score;     // Average health across all indexes
        ULONG databases_needing_attention; // Databases with health issues
        
        GlobalMaintenanceStatistics()
            : total_databases(0), total_indexes(0), active_maintenance_tasks(0),
              completed_tasks_today(0), global_health_score(1.0),
              databases_needing_attention(0)
        {
        }
    };
    
    GlobalMaintenanceStatistics getGlobalStatistics() const;
    
    // Emergency operations
    void triggerEmergencyMaintenance(Database* database, const index_desc* idx);
    void pauseAllMaintenance();
    void resumeAllMaintenance();

private:
    GlobalBitmapMaintenanceManager();
    ~GlobalBitmapMaintenanceManager();
    
    static GlobalBitmapMaintenanceManager* s_instance;
    static ScratchBird::Mutex s_instance_mutex;
    
    struct DatabaseScheduler {
        Database* database;
        std::unique_ptr<BitmapIndexMaintenanceScheduler> scheduler;
        
        DatabaseScheduler(Database* db) : database(db) {}
    };
    
    std::vector<DatabaseScheduler> m_database_schedulers;
    mutable ScratchBird::Mutex m_schedulers_mutex;
    
    MaintenanceScheduleConfig m_global_config;
    bool m_maintenance_paused;
    
    // Global monitoring
    void globalMonitoringThreadProc();
    bool m_global_monitoring_active;
    
    DatabaseScheduler* findSchedulerForDatabase(Database* database);
    void cleanupUnusedSchedulers();
};

//----------------------------
// Integration Hooks
//----------------------------

/**
 * Integration points for bitmap index maintenance
 */
class BitmapMaintenanceIntegration
{
public:
    // System event hooks
    static void onSystemStartup();
    static void onSystemShutdown();
    static void onDatabaseOpen(Database* database);
    static void onDatabaseClose(Database* database);
    
    // Index event hooks
    static void onIndexCreated(thread_db* tdbb, const index_desc* idx);
    static void onIndexDropped(thread_db* tdbb, const index_desc* idx);
    static void onIndexUpdated(thread_db* tdbb, const index_desc* idx, ULONG update_count);
    static void onIndexAccessed(thread_db* tdbb, const index_desc* idx);
    
    // Configuration management
    static void loadMaintenanceConfiguration(Database* database);
    static void saveMaintenanceConfiguration(Database* database);
    
    // Monitoring integration
    static void reportMaintenanceMetrics(const BitmapIndexMaintenanceScheduler::SchedulerStatistics& stats);
    static void alertOnMaintenanceIssues(Database* database, const ScratchBird::string& issue_description);

private:
    static bool s_hooks_registered;
    static ScratchBird::Mutex s_integration_mutex;
};

//----------------------------
// Utility Functions
//----------------------------

// Health assessment utilities
IndexHealthMetrics calculateIndexHealth(thread_db* tdbb, const index_desc* idx);
double calculateOverallHealthScore(const IndexHealthMetrics& metrics);
bool requiresImmediateMaintenance(const IndexHealthMetrics& metrics);

// Scheduling utilities
GDS_TIMESTAMP findNextMaintenanceWindow(const MaintenanceScheduleConfig& config, GDS_TIMESTAMP after_time);
bool isInMaintenanceWindow(const MaintenanceScheduleConfig& config, GDS_TIMESTAMP timestamp);
ULONG estimateTaskDuration(MaintenanceTaskType task_type, const IndexHealthMetrics& metrics);

// Task management utilities
MaintenancePriority calculateTaskPriority(const IndexHealthMetrics& metrics, MaintenanceTaskType task_type);
bool canTasksRunConcurrently(const MaintenanceTask& task1, const MaintenanceTask& task2);
std::vector<MaintenanceTask> optimizeTaskSchedule(const std::vector<MaintenanceTask>& tasks);

// Resource estimation
ULONG estimateMemoryRequirement(const MaintenanceTask& task);
ULONG estimateCpuRequirement(const MaintenanceTask& task);
ULONG estimateIoRequirement(const MaintenanceTask& task);

} // namespace Jrd

#endif // JRD_BITMAP_INDEX_MAINTENANCE_SCHEDULER_H
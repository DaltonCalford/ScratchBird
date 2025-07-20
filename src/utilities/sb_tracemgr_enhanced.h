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
    class TraceManager;
}

class SBEngineIntegration;

namespace SBEnhanced {

// Trace session types
enum class TraceSessionType {
    DATABASE = 0,       // Database-level tracing
    CONNECTION = 1,     // Connection-level tracing
    TRANSACTION = 2,    // Transaction-level tracing
    STATEMENT = 3,      // Statement-level tracing
    PROCEDURE = 4,      // Stored procedure tracing
    FUNCTION = 5,       // Function tracing
    TRIGGER = 6,        // Trigger tracing
    SERVICE = 7,        // Service operation tracing
    SYSTEM = 8,         // System-wide tracing
    CUSTOM = 255        // Custom trace session
};

// Trace event types
enum class TraceEventType {
    UNKNOWN = 0,
    CONNECTION_START = 1,       // Connection established
    CONNECTION_END = 2,         // Connection closed
    TRANSACTION_START = 3,      // Transaction started
    TRANSACTION_COMMIT = 4,     // Transaction committed
    TRANSACTION_ROLLBACK = 5,   // Transaction rolled back
    STATEMENT_START = 6,        // Statement execution started
    STATEMENT_FINISH = 7,       // Statement execution finished
    STATEMENT_FREE = 8,         // Statement resources freed
    PROCEDURE_START = 9,        // Procedure execution started
    PROCEDURE_FINISH = 10,      // Procedure execution finished
    FUNCTION_START = 11,        // Function execution started
    FUNCTION_FINISH = 12,       // Function execution finished
    TRIGGER_START = 13,         // Trigger execution started
    TRIGGER_FINISH = 14,        // Trigger execution finished
    SERVICE_START = 15,         // Service operation started
    SERVICE_FINISH = 16,        // Service operation finished
    ERROR = 17,                 // Error occurred
    WARNING = 18,               // Warning issued
    SWEEP_START = 19,           // Database sweep started
    SWEEP_FINISH = 20,          // Database sweep finished
    SORT_START = 21,            // Sort operation started
    SORT_FINISH = 22,           // Sort operation finished
    BLR_COMPILE = 23,          // BLR compilation
    BLR_EXECUTE = 24,          // BLR execution
    DYN_EXECUTE = 25,          // Dynamic SQL execution
    CONTEXT_VARS = 26,         // Context variables
    CUSTOM_EVENT = 255         // Custom event type
};

// Trace analysis types
enum class TraceAnalysisType {
    PERFORMANCE = 0,    // Performance analysis
    SECURITY = 1,       // Security analysis
    RESOURCE = 2,       // Resource usage analysis
    BEHAVIOR = 3,       // Behavioral pattern analysis
    ANOMALY = 4,        // Anomaly detection
    TREND = 5,          // Trend analysis
    BOTTLENECK = 6,     // Bottleneck identification
    OPTIMIZATION = 7,   // Optimization recommendations
    COMPLIANCE = 8,     // Compliance checking
    FORENSIC = 9        // Forensic analysis
};

// Trace filtering options
enum class TraceFilterType {
    NONE = 0,           // No filtering
    USER = 1,           // Filter by user
    DATABASE = 2,       // Filter by database
    APPLICATION = 3,    // Filter by application
    TIME_RANGE = 4,     // Filter by time range
    EVENT_TYPE = 5,     // Filter by event type
    DURATION = 6,       // Filter by duration
    ERROR_ONLY = 7,     // Show errors only
    SLOW_QUERIES = 8,   // Show slow queries only
    CUSTOM = 255        // Custom filter
};

// Trace session information
struct TraceSession {
    uint64_t session_id = 0;
    std::string session_name;
    TraceSessionType session_type = TraceSessionType::DATABASE;
    
    std::string database_path;
    std::string target_user;
    std::string target_application;
    std::string target_connection;
    
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::chrono::milliseconds session_duration{0};
    
    bool is_active = false;
    bool is_paused = false;
    bool auto_flush = true;
    uint32_t flush_interval_seconds = 30;
    
    std::string trace_config;
    std::string output_file_path;
    uint64_t max_file_size_mb = 100;
    bool rotate_files = true;
    uint32_t max_file_count = 10;
    
    uint64_t events_captured = 0;
    uint64_t events_filtered = 0;
    uint64_t bytes_written = 0;
    double capture_rate_per_second = 0.0;
    
    std::vector<TraceEventType> enabled_events;
    std::vector<TraceFilterType> active_filters;
    std::map<std::string, std::string> filter_parameters;
    std::map<std::string, std::string> session_options;
    
    std::string getSessionTypeString() const;
    bool isEventEnabled(TraceEventType event_type) const;
    std::chrono::milliseconds getSessionDuration() const;
    double getEventCaptureRate() const;
    uint64_t getEstimatedFileSize() const;
};

// Individual trace event
struct TraceEvent {
    uint64_t event_id = 0;
    uint64_t session_id = 0;
    TraceEventType event_type = TraceEventType::UNKNOWN;
    
    std::chrono::system_clock::time_point timestamp;
    std::chrono::microseconds event_duration{0};
    
    std::string database_name;
    std::string user_name;
    std::string application_name;
    std::string connection_id;
    std::string transaction_id;
    std::string statement_id;
    
    std::string sql_text;
    std::string procedure_name;
    std::string function_name;
    std::string trigger_name;
    std::string table_name;
    
    uint64_t records_affected = 0;
    uint64_t records_fetched = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t memory_used = 0;
    uint64_t disk_reads = 0;
    uint64_t disk_writes = 0;
    
    uint32_t plan_hash = 0;
    std::string execution_plan;
    std::string error_message;
    std::string warning_message;
    
    double cpu_time_ms = 0.0;
    double elapsed_time_ms = 0.0;
    double wait_time_ms = 0.0;
    uint32_t lock_count = 0;
    uint32_t sort_count = 0;
    
    std::map<std::string, std::string> context_variables;
    std::map<std::string, std::string> custom_attributes;
    
    std::string getEventTypeString() const;
    bool isSlowEvent(double threshold_ms = 1000.0) const;
    bool isErrorEvent() const;
    bool isResourceIntensive() const;
    double getResourceScore() const;
};

// Trace analysis result
struct TraceAnalysisResult {
    std::chrono::system_clock::time_point analysis_time;
    TraceAnalysisType analysis_type = TraceAnalysisType::PERFORMANCE;
    bool analysis_successful = false;
    
    // Session information
    uint64_t session_id = 0;
    std::string session_name;
    std::chrono::system_clock::time_point session_start;
    std::chrono::system_clock::time_point session_end;
    
    // Event statistics
    uint64_t total_events_analyzed = 0;
    uint64_t unique_users = 0;
    uint64_t unique_applications = 0;
    uint64_t unique_connections = 0;
    uint64_t unique_transactions = 0;
    uint64_t unique_statements = 0;
    
    // Performance metrics
    double average_response_time_ms = 0.0;
    double median_response_time_ms = 0.0;
    double percentile_95_response_time_ms = 0.0;
    double percentile_99_response_time_ms = 0.0;
    double maximum_response_time_ms = 0.0;
    double minimum_response_time_ms = 0.0;
    
    // Throughput metrics
    double events_per_second = 0.0;
    double statements_per_second = 0.0;
    double transactions_per_second = 0.0;
    double data_transfer_rate_mbps = 0.0;
    
    // Resource utilization
    double average_cpu_utilization = 0.0;
    double peak_cpu_utilization = 0.0;
    double average_memory_usage_mb = 0.0;
    double peak_memory_usage_mb = 0.0;
    double average_disk_io_mbps = 0.0;
    double peak_disk_io_mbps = 0.0;
    
    // Error and warning analysis
    uint64_t total_errors = 0;
    uint64_t total_warnings = 0;
    double error_rate = 0.0;
    double warning_rate = 0.0;
    std::vector<std::string> most_common_errors;
    std::vector<std::string> most_common_warnings;
    
    // Query analysis
    std::vector<std::string> slowest_queries;
    std::vector<std::string> most_frequent_queries;
    std::vector<std::string> most_resource_intensive_queries;
    std::vector<std::string> problematic_query_patterns;
    
    // User and application analysis
    std::vector<std::string> most_active_users;
    std::vector<std::string> most_active_applications;
    std::vector<std::string> problematic_user_patterns;
    std::vector<std::string> suspicious_activities;
    
    // Bottleneck identification
    std::vector<std::string> performance_bottlenecks;
    std::vector<std::string> resource_bottlenecks;
    std::vector<std::string> concurrency_issues;
    std::vector<std::string> locking_issues;
    
    // Optimization recommendations
    std::vector<std::string> performance_recommendations;
    std::vector<std::string> index_recommendations;
    std::vector<std::string> query_optimization_suggestions;
    std::vector<std::string> configuration_recommendations;
    std::vector<std::string> capacity_planning_suggestions;
    
    // Trend analysis
    std::vector<std::string> performance_trends;
    std::vector<std::string> usage_trends;
    std::vector<std::string> growth_projections;
    
    // Security analysis
    std::vector<std::string> security_findings;
    std::vector<std::string> access_patterns;
    std::vector<std::string> privilege_usage;
    std::vector<std::string> potential_security_risks;
    
    // Issues and warnings
    std::vector<std::string> critical_issues;
    std::vector<std::string> analysis_warnings;
    std::vector<std::string> analysis_errors;
    
    std::string analysis_report_path;
    
    std::string generateAnalysisReport() const;
    std::string generatePerformanceReport() const;
    std::string generateSecurityReport() const;
    std::string generateRecommendationsReport() const;
};

// Trace configuration options
struct TraceConfiguration {
    std::string configuration_name;
    TraceSessionType session_type = TraceSessionType::DATABASE;
    
    // Target specification
    std::string database_path;
    std::vector<std::string> target_users;
    std::vector<std::string> target_applications;
    std::vector<std::string> target_connections;
    
    // Event configuration
    std::vector<TraceEventType> enabled_events;
    std::map<TraceEventType, bool> event_details;  // Whether to capture detailed info
    
    // Filtering options
    std::vector<TraceFilterType> active_filters;
    std::map<std::string, std::string> filter_parameters;
    double min_duration_ms = 0.0;      // Minimum duration to capture
    double max_duration_ms = 0.0;      // Maximum duration to capture (0 = no limit)
    bool errors_only = false;
    bool slow_queries_only = false;
    double slow_query_threshold_ms = 1000.0;
    
    // Output configuration
    std::string output_file_path;
    std::string output_format = "BINARY";  // BINARY, TEXT, JSON, XML
    uint64_t max_file_size_mb = 100;
    bool rotate_files = true;
    uint32_t max_file_count = 10;
    bool compress_files = false;
    
    // Performance settings
    bool auto_flush = true;
    uint32_t flush_interval_seconds = 30;
    uint32_t buffer_size_kb = 1024;
    bool use_memory_buffer = true;
    uint32_t max_memory_buffer_mb = 10;
    
    // Advanced options
    bool include_bind_variables = false;
    bool include_execution_plans = false;
    bool include_context_variables = false;
    bool include_blr_requests = false;
    bool include_dyn_requests = false;
    double sampling_rate = 1.0;        // 1.0 = 100%, 0.1 = 10%
    
    // Security and privacy
    bool mask_sensitive_data = false;
    std::vector<std::string> sensitive_tables;
    std::vector<std::string> sensitive_columns;
    bool encrypt_output = false;
    std::string encryption_key;
    
    // Session management
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::chrono::hours max_session_duration{24};  // Maximum session duration
    bool auto_stop_on_error = false;
    uint32_t max_error_count = 100;
    
    std::map<std::string, std::string> custom_options;
    
    bool isValidConfiguration() const;
    std::string validateConfiguration() const;
};

// Trace monitoring progress
struct TraceMonitoringProgress {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point current_time;
    
    uint64_t sessions_monitored = 0;
    uint64_t events_processed = 0;
    uint64_t events_filtered = 0;
    uint64_t bytes_captured = 0;
    uint64_t files_created = 0;
    
    std::string current_session;
    std::string current_operation;
    bool monitoring_active = false;
    
    std::chrono::seconds getMonitoringDuration() const;
    double getEventsPerSecond() const;
    double getCaptureRateMBps() const;
    std::string getProgressSummary() const;
};

// Trace management result
struct TraceManagementResult {
    TraceConfiguration trace_config;
    TraceMonitoringProgress monitoring_progress;
    
    bool operation_successful = false;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    
    // Session results
    std::vector<uint64_t> created_session_ids;
    std::vector<uint64_t> stopped_session_ids;
    std::vector<TraceSession> session_results;
    
    // Analysis results
    std::vector<TraceAnalysisResult> analysis_results;
    std::vector<std::string> generated_reports;
    
    // Performance metrics
    uint64_t total_events_captured = 0;
    uint64_t total_bytes_captured = 0;
    uint32_t sessions_processed = 0;
    std::chrono::milliseconds total_processing_time{0};
    double average_capture_rate_per_second = 0.0;
    
    // Issues and warnings
    std::vector<std::string> operation_warnings;
    std::vector<std::string> operation_errors;
    std::vector<std::string> configuration_issues;
    
    std::string management_report_path;
    
    std::chrono::milliseconds getOperationDuration() const;
    std::string generateManagementReport() const;
};

} // namespace SBEnhanced

// Main enhanced Trace Manager utility class
class TraceMgrEnhanced {
private:
    std::unique_ptr<SBEngineIntegration> engine;
    std::unique_ptr<jrd::TraceManager> trace_manager;
    std::atomic<bool> trace_active{false};
    std::atomic<bool> shutdown_requested{false};
    
    // Trace session management
    std::map<uint64_t, SBEnhanced::TraceSession> active_sessions;
    std::vector<SBEnhanced::TraceEvent> captured_events;
    std::atomic<uint64_t> next_session_id{1};
    std::atomic<uint64_t> next_event_id{1};
    
    // Thread management
    std::unique_ptr<std::thread> monitoring_thread;
    std::unique_ptr<std::thread> analysis_thread;
    std::vector<std::unique_ptr<std::thread>> capture_threads;
    std::mutex trace_data_mutex;
    std::mutex session_mutex;
    std::mutex analysis_mutex;
    
    // Configuration and state
    SBEnhanced::TraceConfiguration current_config;
    SBEnhanced::TraceMonitoringProgress current_progress;
    
    // Internal state
    std::vector<std::string> error_log;
    std::vector<std::string> warning_log;
    std::string last_error;

public:
    // Constructor and destructor
    TraceMgrEnhanced();
    ~TraceMgrEnhanced();
    
    // === ORIGINAL TRACE MANAGER FUNCTIONALITY (100% Compatible) ===
    
    // Basic trace operations
    uint64_t startTrace(const std::string& trace_config);
    bool stopTrace(uint64_t session_id);
    bool suspendTrace(uint64_t session_id);
    bool resumeTrace(uint64_t session_id);
    bool listTraces(std::vector<std::string>& trace_list);
    
    // === ENHANCED FUNCTIONALITY ===
    
    // Advanced trace session management
    uint64_t startTraceSession(const SBEnhanced::TraceConfiguration& config,
                              SBEnhanced::TraceManagementResult& result);
    
    bool stopTraceSession(uint64_t session_id,
                         SBEnhanced::TraceManagementResult& result);
    
    bool pauseTraceSession(uint64_t session_id);
    bool resumeTraceSession(uint64_t session_id);
    bool restartTraceSession(uint64_t session_id);
    
    // Trace session querying and control
    bool getTraceSession(uint64_t session_id, SBEnhanced::TraceSession& session_info);
    
    bool listActiveSessions(std::vector<SBEnhanced::TraceSession>& sessions);
    bool listCompletedSessions(std::vector<SBEnhanced::TraceSession>& sessions);
    
    bool getSessionsByType(SBEnhanced::TraceSessionType session_type,
                          std::vector<SBEnhanced::TraceSession>& sessions);
    
    bool getSessionsByDatabase(const std::string& database_path,
                              std::vector<SBEnhanced::TraceSession>& sessions);
    
    bool flushTraceSession(uint64_t session_id);
    bool rotateTraceFile(uint64_t session_id);
    
    // Trace event querying and analysis
    bool getTraceEvents(uint64_t session_id,
                       std::vector<SBEnhanced::TraceEvent>& events);
    
    bool getEventsByType(uint64_t session_id,
                        SBEnhanced::TraceEventType event_type,
                        std::vector<SBEnhanced::TraceEvent>& events);
    
    bool getEventsByTimeRange(uint64_t session_id,
                             const std::chrono::system_clock::time_point& start_time,
                             const std::chrono::system_clock::time_point& end_time,
                             std::vector<SBEnhanced::TraceEvent>& events);
    
    bool getSlowEvents(uint64_t session_id,
                      double threshold_ms,
                      std::vector<SBEnhanced::TraceEvent>& slow_events);
    
    bool getErrorEvents(uint64_t session_id,
                       std::vector<SBEnhanced::TraceEvent>& error_events);
    
    // Comprehensive trace analysis
    bool analyzeTraceSession(uint64_t session_id,
                            SBEnhanced::TraceAnalysisType analysis_type,
                            SBEnhanced::TraceAnalysisResult& analysis_result);
    
    bool analyzePerformance(uint64_t session_id,
                           SBEnhanced::TraceAnalysisResult& performance_analysis);
    
    bool analyzeSecurity(uint64_t session_id,
                        SBEnhanced::TraceAnalysisResult& security_analysis);
    
    bool analyzeResourceUsage(uint64_t session_id,
                             SBEnhanced::TraceAnalysisResult& resource_analysis);
    
    bool detectAnomalies(uint64_t session_id,
                        std::vector<std::string>& anomalies);
    
    bool identifyBottlenecks(uint64_t session_id,
                            std::vector<std::string>& bottlenecks);
    
    // Advanced analysis and reporting
    bool generatePerformanceReport(uint64_t session_id,
                                  const std::string& report_format = "HTML",
                                  const std::string& output_path = "");
    
    bool generateSecurityReport(uint64_t session_id,
                               const std::string& report_format = "HTML",
                               const std::string& output_path = "");
    
    bool generateComprehensiveReport(uint64_t session_id,
                                    const std::string& report_format = "HTML",
                                    const std::string& output_path = "");
    
    bool compareTraceSessions(uint64_t session_id1,
                             uint64_t session_id2,
                             std::vector<std::string>& comparison_results);
    
    bool createPerformanceDashboard(const std::vector<uint64_t>& session_ids,
                                   const std::string& output_path);
    
    // Query and statement analysis
    bool analyzeSlowQueries(uint64_t session_id,
                           double threshold_ms,
                           std::vector<std::string>& analysis_results);
    
    bool identifyQueryPatterns(uint64_t session_id,
                              std::vector<std::string>& patterns);
    
    bool suggestQueryOptimizations(uint64_t session_id,
                                  std::vector<std::string>& optimizations);
    
    bool analyzeExecutionPlans(uint64_t session_id,
                              std::vector<std::string>& plan_analysis);
    
    // User and application behavior analysis
    bool analyzeUserBehavior(uint64_t session_id,
                            const std::string& user_name,
                            std::vector<std::string>& behavior_analysis);
    
    bool analyzeApplicationBehavior(uint64_t session_id,
                                   const std::string& application_name,
                                   std::vector<std::string>& behavior_analysis);
    
    bool detectSuspiciousActivity(uint64_t session_id,
                                 std::vector<std::string>& suspicious_activities);
    
    // Performance monitoring and alerting
    bool enableRealTimeMonitoring(uint64_t session_id,
                                 const std::function<void(const SBEnhanced::TraceEvent&)>& event_callback);
    
    bool disableRealTimeMonitoring(uint64_t session_id);
    
    bool setPerformanceAlert(uint64_t session_id,
                           const std::string& alert_condition,
                           const std::function<void(const SBEnhanced::TraceEvent&)>& alert_callback);
    
    bool removePerformanceAlert(uint64_t session_id, const std::string& alert_condition);
    
    // Trace data export and import
    bool exportTraceData(uint64_t session_id,
                        const std::string& export_format,
                        const std::string& output_path);
    
    bool importTraceData(const std::string& import_path,
                        uint64_t& new_session_id);
    
    bool mergeTraceSessions(const std::vector<uint64_t>& session_ids,
                           uint64_t& merged_session_id);
    
    bool splitTraceSession(uint64_t session_id,
                          const std::chrono::system_clock::time_point& split_time,
                          uint64_t& new_session_id);
    
    // Configuration and template management
    bool saveTraceConfiguration(const std::string& config_name,
                               const SBEnhanced::TraceConfiguration& config);
    
    bool loadTraceConfiguration(const std::string& config_name,
                               SBEnhanced::TraceConfiguration& config);
    
    bool deleteTraceConfiguration(const std::string& config_name);
    
    bool listTraceConfigurations(std::vector<std::string>& config_names);
    
    // Trace session archival and cleanup
    bool archiveTraceSession(uint64_t session_id, const std::string& archive_path);
    bool restoreTraceSession(const std::string& archive_path, uint64_t& session_id);
    
    bool cleanupOldSessions(const std::chrono::hours& retention_period);
    bool compressTraceFiles(uint64_t session_id);
    
    // Statistical analysis
    bool generateTraceStatistics(uint64_t session_id,
                                 std::map<std::string, double>& statistics);
    
    bool analyzePerformanceTrends(const std::vector<uint64_t>& session_ids,
                                 const std::chrono::hours& time_window,
                                 std::vector<std::string>& trend_analysis);
    
    bool predictPerformanceIssues(uint64_t session_id,
                                 std::vector<std::string>& predictions);
    
    // Progress monitoring and control
    SBEnhanced::TraceMonitoringProgress getCurrentProgress() const;
    bool isTraceManagerActive() const;
    void requestShutdown();
    
    // Error handling and logging
    std::vector<std::string> getErrors() const;
    std::vector<std::string> getWarnings() const;
    std::string getLastError() const;
    void clearErrorLog();
    
    // Configuration and validation
    bool validateTraceConfiguration(const SBEnhanced::TraceConfiguration& config);
    bool testTraceConnection(const SBEnhanced::TraceConfiguration& config);

private:
    // Internal initialization
    bool initializeEngine();
    bool initializeTraceManager();
    
    // Trace session management
    void traceMonitoringLoop();
    void traceAnalysisLoop();
    void eventCaptureLoop(uint64_t session_id);
    
    // Core trace operations
    bool startTraceSessionInternal(const SBEnhanced::TraceConfiguration& config,
                                  uint64_t& session_id);
    
    bool stopTraceSessionInternal(uint64_t session_id);
    
    bool processTraceEvent(const SBEnhanced::TraceEvent& event);
    
    // Event processing and filtering
    bool applyEventFilters(const SBEnhanced::TraceEvent& event,
                          const SBEnhanced::TraceConfiguration& config);
    
    bool shouldCaptureEvent(const SBEnhanced::TraceEvent& event,
                           const SBEnhanced::TraceConfiguration& config);
    
    bool processEventForAnalysis(const SBEnhanced::TraceEvent& event,
                                uint64_t session_id);
    
    // Analysis algorithms
    bool performPerformanceAnalysis(uint64_t session_id,
                                   SBEnhanced::TraceAnalysisResult& result);
    
    bool performSecurityAnalysis(uint64_t session_id,
                                SBEnhanced::TraceAnalysisResult& result);
    
    bool performResourceAnalysis(uint64_t session_id,
                                SBEnhanced::TraceAnalysisResult& result);
    
    bool detectPerformanceAnomalies(const std::vector<SBEnhanced::TraceEvent>& events,
                                   std::vector<std::string>& anomalies);
    
    // File management
    bool createTraceFile(uint64_t session_id, const SBEnhanced::TraceConfiguration& config);
    bool rotateTraceFileInternal(uint64_t session_id);
    bool flushTraceFileInternal(uint64_t session_id);
    
    // Progress tracking helpers
    void updateProgress(const std::string& operation, const std::string& session);
    void logError(const std::string& operation, const std::string& error);
    void logWarning(const std::string& operation, const std::string& warning);
    void logInfo(const std::string& operation, const std::string& info);
    
    // Configuration helpers
    bool validateConfigurationInternal(const SBEnhanced::TraceConfiguration& config,
                                     std::vector<std::string>& issues);
    
    bool applyConfiguration(const SBEnhanced::TraceConfiguration& config);
    
    // Database connection helpers
    bool establishTraceConnection(const SBEnhanced::TraceConfiguration& config,
                                std::unique_ptr<jrd::TraceManager>& trace_mgr);
    
    void closeTraceConnection(std::unique_ptr<jrd::TraceManager>& trace_mgr);
    
    // Analysis helpers
    bool calculatePerformanceMetrics(const std::vector<SBEnhanced::TraceEvent>& events,
                                    SBEnhanced::TraceAnalysisResult& result);
    
    bool identifySlowQueries(const std::vector<SBEnhanced::TraceEvent>& events,
                           double threshold_ms,
                           std::vector<std::string>& slow_queries);
    
    bool analyzeUserPatterns(const std::vector<SBEnhanced::TraceEvent>& events,
                           std::vector<std::string>& patterns);
    
    // Report generation helpers
    std::string generateHTMLReport(const SBEnhanced::TraceAnalysisResult& analysis) const;
    std::string generateJSONReport(const SBEnhanced::TraceAnalysisResult& analysis) const;
    std::string generateTextReport(const SBEnhanced::TraceAnalysisResult& analysis) const;
    
    // Utility helpers
    SBEnhanced::TraceEvent parseTraceRecord(const std::string& trace_record);
    std::string formatTraceEvent(const SBEnhanced::TraceEvent& event) const;
    double calculateEventResourceScore(const SBEnhanced::TraceEvent& event) const;
};

// Utility functions for enhanced Trace Manager
namespace SBEnhanced {

// Quick trace operations
uint64_t quickStartTrace(const std::string& database_path,
                        const std::string& output_path);

bool quickStopTrace(uint64_t session_id);

bool quickAnalyzeTrace(uint64_t session_id, const std::string& analysis_type);

// Trace event helpers
std::string formatTraceDuration(const std::chrono::microseconds& duration);
std::string formatTraceSize(uint64_t size_bytes);
double calculateTraceEfficiency(const TraceAnalysisResult& analysis);

// Performance analysis utilities
bool isPerformanceOptimal(const TraceAnalysisResult& analysis);
std::vector<std::string> identifyPerformanceIssues(const TraceAnalysisResult& analysis);
double calculateOverallPerformanceScore(const TraceAnalysisResult& analysis);

// Security analysis utilities
bool hasSecurityConcerns(const TraceAnalysisResult& analysis);
std::vector<std::string> identifySecurityRisks(const TraceAnalysisResult& analysis);
uint32_t calculateSecurityScore(const TraceAnalysisResult& analysis);

// Export utilities
bool exportToCSV(const std::vector<TraceEvent>& events, const std::string& filename);
bool exportToJSON(const std::vector<TraceEvent>& events, const std::string& filename);
bool exportToXML(const std::vector<TraceEvent>& events, const std::string& filename);

// Compatibility helpers for command-line usage
int parseTraceMgrCommandLine(int argc, char* argv[],
                            TraceConfiguration& config);

bool executeClassicTraceMgrCommand(const std::string& command_line);

} // namespace SBEnhanced
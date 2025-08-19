#pragma once

#include "sb_engine_integration.h"
#include "utility_enhancements.h"
#include "utility_config.h"

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <set>
#include <unordered_map>

namespace SBEnhanced {

// Statistics categories
enum class StatCategory {
    DATABASE_OVERVIEW,
    TABLE_STATISTICS,
    INDEX_STATISTICS,
    TRANSACTION_STATISTICS,
    CONNECTION_STATISTICS,
    PERFORMANCE_COUNTERS,
    STORAGE_STATISTICS,
    CACHE_STATISTICS,
    LOCK_STATISTICS,
    BACKUP_STATISTICS,
    REPLICATION_STATISTICS,
    SECURITY_STATISTICS,
    SYSTEM_STATISTICS,
    CUSTOM_STATISTICS
};

// Statistics collection modes
enum class CollectionMode {
    SNAPSHOT,           // Single point-in-time collection
    CONTINUOUS,         // Continuous monitoring
    HISTORICAL,         // Historical analysis
    COMPARATIVE,        // Compare multiple timepoints
    REAL_TIME,          // Real-time streaming
    BATCH,              // Batch processing
    INTERACTIVE         // Interactive analysis
};

// Output formats for statistics
enum class StatOutputFormat {
    TABLE,              // Formatted table
    CSV,                // Comma-separated values
    JSON,               // JSON format
    XML,                // XML format
    HTML,               // HTML report
    MARKDOWN,           // Markdown format
    YAML,               // YAML format
    EXCEL,              // Excel format
    PDF,                // PDF report
    GRAFANA,            // Grafana dashboard
    PROMETHEUS,         // Prometheus metrics
    CUSTOM              // Custom format
};

// Analysis types
enum class AnalysisType {
    BASIC,              // Basic statistics
    DETAILED,           // Detailed analysis
    PERFORMANCE,        // Performance analysis
    OPTIMIZATION,       // Optimization recommendations
    TREND_ANALYSIS,     // Trend analysis
    PREDICTIVE,         // Predictive analysis
    COMPARATIVE,        // Comparative analysis
    HEALTH_CHECK,       // Health check analysis
    CAPACITY_PLANNING,  // Capacity planning
    SECURITY_AUDIT      // Security audit
};

// Database health status
enum class HealthStatus {
    EXCELLENT,          // Optimal performance
    GOOD,               // Good performance
    WARNING,            // Performance concerns
    CRITICAL,           // Critical issues
    UNKNOWN             // Unable to determine
};

// Performance metrics
struct PerformanceMetrics {
    double cpu_usage_percent = 0.0;
    uint64_t memory_usage_bytes = 0;
    uint64_t disk_io_reads = 0;
    uint64_t disk_io_writes = 0;
    uint64_t network_io_bytes = 0;
    uint64_t cache_hit_ratio = 0;
    uint64_t page_reads = 0;
    uint64_t page_writes = 0;
    uint64_t page_fetches = 0;
    uint64_t page_marks = 0;
    double average_query_time = 0.0;
    uint64_t active_connections = 0;
    uint64_t total_connections = 0;
    uint64_t deadlocks = 0;
    uint64_t lock_waits = 0;
    uint64_t transactions_per_second = 0;
    uint64_t selects_per_second = 0;
    uint64_t inserts_per_second = 0;
    uint64_t updates_per_second = 0;
    uint64_t deletes_per_second = 0;
    std::chrono::system_clock::time_point timestamp;
};

// Database statistics
struct DatabaseStatistics {
    std::string database_name;
    std::string database_path;
    uint64_t database_size_bytes = 0;
    uint64_t pages_allocated = 0;
    uint64_t pages_used = 0;
    uint64_t page_size = 0;
    uint64_t table_count = 0;
    uint64_t index_count = 0;
    uint64_t view_count = 0;
    uint64_t procedure_count = 0;
    uint64_t function_count = 0;
    uint64_t trigger_count = 0;
    uint64_t domain_count = 0;
    uint64_t generator_count = 0;
    uint64_t role_count = 0;
    uint64_t user_count = 0;
    uint64_t schema_count = 0;
    std::string ods_version;
    std::string database_version;
    std::string creation_date;
    std::string backup_date;
    std::vector<std::string> attached_users;
    std::vector<std::string> active_transactions;
    PerformanceMetrics performance;
    std::map<std::string, std::string> configuration;
    std::chrono::system_clock::time_point collection_time;
};

// Table statistics
struct TableStatistics {
    std::string table_name;
    std::string schema_name;
    uint64_t record_count = 0;
    uint64_t data_pages = 0;
    uint64_t index_pages = 0;
    uint64_t blob_pages = 0;
    uint64_t total_size_bytes = 0;
    uint64_t average_record_size = 0;
    uint64_t fragmentation_percent = 0;
    double fill_factor = 0.0;
    std::vector<std::string> indexes;
    std::vector<std::string> constraints;
    std::vector<std::string> triggers;
    std::map<std::string, uint64_t> column_statistics;
    std::chrono::system_clock::time_point last_analyzed;
    std::chrono::system_clock::time_point last_updated;
};

// Index statistics
struct IndexStatistics {
    std::string index_name;
    std::string table_name;
    std::string schema_name;
    std::vector<std::string> columns;
    bool is_unique = false;
    bool is_primary = false;
    bool is_foreign = false;
    uint64_t pages_allocated = 0;
    uint64_t pages_used = 0;
    uint64_t key_count = 0;
    uint64_t duplicate_keys = 0;
    double selectivity = 0.0;
    uint64_t access_count = 0;
    uint64_t scan_count = 0;
    uint64_t seek_count = 0;
    double average_seek_time = 0.0;
    uint64_t fragmentation_percent = 0;
    std::chrono::system_clock::time_point last_rebuilt;
    std::chrono::system_clock::time_point last_analyzed;
};

// Transaction statistics
struct TransactionStatistics {
    uint64_t active_transactions = 0;
    uint64_t committed_transactions = 0;
    uint64_t rolled_back_transactions = 0;
    uint64_t read_only_transactions = 0;
    uint64_t read_write_transactions = 0;
    uint64_t long_running_transactions = 0;
    double average_transaction_time = 0.0;
    uint64_t deadlocks_detected = 0;
    uint64_t lock_timeouts = 0;
    uint64_t concurrent_transactions = 0;
    std::map<std::string, uint64_t> isolation_levels;
    std::vector<std::string> oldest_active_transactions;
    std::chrono::system_clock::time_point collection_time;
};

// Connection statistics
struct ConnectionStatistics {
    uint64_t total_connections = 0;
    uint64_t active_connections = 0;
    uint64_t idle_connections = 0;
    uint64_t failed_connections = 0;
    uint64_t rejected_connections = 0;
    double average_connection_time = 0.0;
    uint64_t peak_connections = 0;
    std::map<std::string, uint64_t> connections_by_user;
    std::map<std::string, uint64_t> connections_by_application;
    std::map<std::string, uint64_t> connections_by_protocol;
    std::vector<std::string> long_running_connections;
    std::chrono::system_clock::time_point collection_time;
};

// Storage statistics
struct StorageStatistics {
    uint64_t total_database_size = 0;
    uint64_t data_size = 0;
    uint64_t index_size = 0;
    uint64_t blob_size = 0;
    uint64_t log_size = 0;
    uint64_t temp_space_size = 0;
    uint64_t free_space = 0;
    uint64_t fragmentation_percent = 0;
    double growth_rate_per_day = 0.0;
    std::map<std::string, uint64_t> file_sizes;
    std::map<std::string, uint64_t> tablespace_usage;
    std::chrono::system_clock::time_point collection_time;
};

// Cache statistics
struct CacheStatistics {
    uint64_t cache_size_bytes = 0;
    uint64_t cache_used_bytes = 0;
    uint64_t cache_hit_ratio = 0;
    uint64_t cache_reads = 0;
    uint64_t cache_writes = 0;
    uint64_t page_buffer_reads = 0;
    uint64_t page_buffer_writes = 0;
    uint64_t metadata_cache_hits = 0;
    uint64_t metadata_cache_misses = 0;
    uint64_t sort_cache_hits = 0;
    uint64_t sort_cache_misses = 0;
    std::map<std::string, uint64_t> cache_by_type;
    std::chrono::system_clock::time_point collection_time;
};

// Lock statistics
struct LockStatistics {
    uint64_t total_locks = 0;
    uint64_t exclusive_locks = 0;
    uint64_t shared_locks = 0;
    uint64_t intent_locks = 0;
    uint64_t deadlocks = 0;
    uint64_t lock_waits = 0;
    uint64_t lock_timeouts = 0;
    double average_lock_wait_time = 0.0;
    std::map<std::string, uint64_t> locks_by_object;
    std::map<std::string, uint64_t> locks_by_transaction;
    std::vector<std::string> blocking_transactions;
    std::chrono::system_clock::time_point collection_time;
};

// Analysis result
struct AnalysisResult {
    AnalysisType type;
    HealthStatus health_status;
    std::string summary;
    std::vector<std::string> findings;
    std::vector<std::string> recommendations;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    std::map<std::string, double> metrics;
    std::map<std::string, std::string> details;
    double confidence_score = 0.0;
    std::chrono::system_clock::time_point analysis_time;
};

// Monitoring configuration
struct MonitoringConfig {
    std::chrono::seconds collection_interval{60};
    std::chrono::seconds retention_period{86400 * 30}; // 30 days
    std::set<StatCategory> enabled_categories;
    std::map<std::string, std::string> thresholds;
    std::vector<std::string> alert_recipients;
    std::string storage_path;
    bool enable_real_time = false;
    bool enable_alerts = false;
    bool enable_web_interface = false;
    int web_port = 8080;
    std::string web_bind_address = "127.0.0.1";
};

// Statistics collection options
struct StatisticsOptions {
    CollectionMode mode = CollectionMode::SNAPSHOT;
    std::set<StatCategory> categories;
    StatOutputFormat output_format = StatOutputFormat::TABLE;
    std::string output_file;
    bool include_system_tables = false;
    bool include_detailed_info = true;
    bool include_recommendations = true;
    bool include_history = false;
    bool verbose = false;
    std::string schema_filter;
    std::string table_filter;
    std::string time_range;
    int max_results = 1000;
    bool sort_results = true;
    std::string sort_column;
    bool ascending = true;
};

// Historical data point
struct HistoricalDataPoint {
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, double> metrics;
    std::string category;
    std::string object_name;
};

// Trend analysis result
struct TrendAnalysis {
    std::string metric_name;
    std::vector<HistoricalDataPoint> data_points;
    double trend_slope = 0.0;
    double correlation_coefficient = 0.0;
    std::string trend_direction; // "increasing", "decreasing", "stable"
    std::string prediction;
    double confidence_level = 0.0;
    std::chrono::system_clock::time_point analysis_time;
};

// Alert configuration
struct AlertConfig {
    std::string alert_name;
    std::string metric_name;
    std::string condition; // "greater_than", "less_than", "equals", "not_equals"
    double threshold_value = 0.0;
    std::chrono::seconds check_interval{60};
    std::chrono::seconds cooldown_period{300};
    std::vector<std::string> recipients;
    std::string message_template;
    bool enabled = true;
    std::chrono::system_clock::time_point last_fired;
};

// Report configuration
struct ReportConfig {
    std::string report_name;
    std::string template_path;
    StatOutputFormat format = StatOutputFormat::HTML;
    std::string output_path;
    std::chrono::seconds generation_interval{3600};
    std::set<StatCategory> included_categories;
    std::map<std::string, std::string> parameters;
    bool auto_generate = false;
    std::chrono::system_clock::time_point last_generated;
};

// Web server configuration
struct WebServerConfig {
    std::string bind_address = "127.0.0.1";
    int port = 8080;
    int max_connections = 100;
    int request_timeout_seconds = 30;
    std::string document_root = "./web";
    std::string ssl_cert_file;
    std::string ssl_key_file;
    bool enable_ssl = false;
    bool enable_websockets = true;
    bool enable_cors = true;
    std::string auth_token;
    std::vector<std::string> allowed_origins;
    int worker_threads = 4;
};

} // namespace SBEnhanced

// Enhanced GSTAT class
class GSTATEnhanced {
private:
    // Core components
    std::unique_ptr<SBEngineIntegration> engine;
    std::unique_ptr<OutputFormatter> formatter;
    std::unique_ptr<QueryAnalyzer> analyzer;
    std::unique_ptr<UtilityConfiguration> config;
    
    // Statistics storage
    std::map<std::string, SBEnhanced::DatabaseStatistics> database_stats;
    std::map<std::string, std::vector<SBEnhanced::TableStatistics>> table_stats;
    std::map<std::string, std::vector<SBEnhanced::IndexStatistics>> index_stats;
    std::map<std::string, SBEnhanced::TransactionStatistics> transaction_stats;
    std::map<std::string, SBEnhanced::ConnectionStatistics> connection_stats;
    std::map<std::string, SBEnhanced::StorageStatistics> storage_stats;
    std::map<std::string, SBEnhanced::CacheStatistics> cache_stats;
    std::map<std::string, SBEnhanced::LockStatistics> lock_stats;
    
    // Historical data
    std::vector<SBEnhanced::HistoricalDataPoint> historical_data;
    std::map<std::string, std::vector<SBEnhanced::TrendAnalysis>> trend_analyses;
    
    // Monitoring
    SBEnhanced::MonitoringConfig monitoring_config;
    std::vector<SBEnhanced::AlertConfig> alert_configs;
    std::vector<SBEnhanced::ReportConfig> report_configs;
    
    // Real-time monitoring
    std::atomic<bool> monitoring_active{false};
    std::thread monitoring_thread;
    std::mutex data_mutex;
    std::condition_variable data_available;
    std::queue<SBEnhanced::PerformanceMetrics> metrics_queue;
    
    // Analysis engines
    std::map<SBEnhanced::AnalysisType, std::function<SBEnhanced::AnalysisResult(const SBEnhanced::StatisticsOptions&)>> analyzers;
    
    // Output and reporting
    std::unique_ptr<std::ofstream> output_file;
    std::unique_ptr<std::ofstream> log_file;
    std::string current_database;
    
    // Performance tracking
    std::chrono::steady_clock::time_point session_start_time;
    std::atomic<uint64_t> total_collections{0};
    std::atomic<uint64_t> successful_collections{0};
    std::atomic<uint64_t> failed_collections{0};
    
    // Error handling
    std::vector<std::string> error_log;
    std::mutex error_mutex;
    
    // Web interface (forward declaration to avoid circular dependency)
    class GSTATWebInterface* web_interface;

public:
    GSTATEnhanced();
    ~GSTATEnhanced();
    
    // Initialization and configuration
    bool initialize(const SBEnhanced::ConnectionOptions& options);
    bool loadConfiguration(const std::string& config_file);
    bool shutdown();
    
    // Database connection
    bool connect(const std::string& database_path, const std::string& username = "", 
                const std::string& password = "", const std::string& role = "");
    bool disconnect();
    bool isConnected() const;
    
    // Statistics collection
    bool collectStatistics(const SBEnhanced::StatisticsOptions& options);
    bool collectDatabaseStatistics(SBEnhanced::DatabaseStatistics& stats);
    bool collectTableStatistics(const std::string& table_name, SBEnhanced::TableStatistics& stats);
    bool collectIndexStatistics(const std::string& index_name, SBEnhanced::IndexStatistics& stats);
    bool collectTransactionStatistics(SBEnhanced::TransactionStatistics& stats);
    bool collectConnectionStatistics(SBEnhanced::ConnectionStatistics& stats);
    bool collectStorageStatistics(SBEnhanced::StorageStatistics& stats);
    bool collectCacheStatistics(SBEnhanced::CacheStatistics& stats);
    bool collectLockStatistics(SBEnhanced::LockStatistics& stats);
    bool collectPerformanceMetrics(SBEnhanced::PerformanceMetrics& metrics);
    
    // Analysis and recommendations
    SBEnhanced::AnalysisResult analyzeDatabase(SBEnhanced::AnalysisType type, const SBEnhanced::StatisticsOptions& options);
    SBEnhanced::AnalysisResult analyzePerformance(const SBEnhanced::StatisticsOptions& options);
    SBEnhanced::AnalysisResult analyzeCapacity(const SBEnhanced::StatisticsOptions& options);
    SBEnhanced::AnalysisResult analyzeHealth(const SBEnhanced::StatisticsOptions& options);
    SBEnhanced::AnalysisResult analyzeTrends(const SBEnhanced::StatisticsOptions& options);
    std::vector<std::string> generateRecommendations(const SBEnhanced::DatabaseStatistics& stats);
    std::vector<std::string> identifyPerformanceIssues(const SBEnhanced::PerformanceMetrics& metrics);
    
    // Historical data management
    bool storeHistoricalData(const SBEnhanced::HistoricalDataPoint& data_point);
    std::vector<SBEnhanced::HistoricalDataPoint> getHistoricalData(const std::string& metric_name, 
                                                                  const std::string& time_range);
    bool purgeHistoricalData(const std::chrono::system_clock::time_point& cutoff_time);
    SBEnhanced::TrendAnalysis analyzeTrend(const std::string& metric_name, const std::string& time_range);
    
    // Real-time monitoring
    bool startMonitoring(const SBEnhanced::MonitoringConfig& config);
    bool stopMonitoring();
    bool isMonitoring() const;
    void monitoringLoop();
    bool addAlert(const SBEnhanced::AlertConfig& alert);
    bool removeAlert(const std::string& alert_name);
    bool checkAlerts();
    
    // Reporting
    bool generateReport(const SBEnhanced::ReportConfig& config);
    bool generateDashboard(const std::string& output_path, SBEnhanced::StatOutputFormat format);
    bool exportStatistics(const SBEnhanced::StatisticsOptions& options);
    bool createCustomReport(const std::string& template_path, const std::string& output_path,
                           const std::map<std::string, std::string>& parameters);
    
    // Output formatting
    std::string formatDatabaseStatistics(const SBEnhanced::DatabaseStatistics& stats, 
                                        SBEnhanced::StatOutputFormat format);
    std::string formatTableStatistics(const std::vector<SBEnhanced::TableStatistics>& stats, 
                                     SBEnhanced::StatOutputFormat format);
    std::string formatIndexStatistics(const std::vector<SBEnhanced::IndexStatistics>& stats, 
                                     SBEnhanced::StatOutputFormat format);
    std::string formatAnalysisResult(const SBEnhanced::AnalysisResult& result, 
                                    SBEnhanced::StatOutputFormat format);
    std::string formatTrendAnalysis(const SBEnhanced::TrendAnalysis& analysis, 
                                   SBEnhanced::StatOutputFormat format);
    
    // Utility methods
    std::string formatBytes(uint64_t bytes);
    std::string formatDuration(const std::chrono::microseconds& duration);
    std::string formatTimestamp(const std::chrono::system_clock::time_point& time);
    std::string formatPercentage(double percentage);
    double calculateFragmentation(uint64_t allocated_pages, uint64_t used_pages);
    double calculateGrowthRate(const std::vector<SBEnhanced::HistoricalDataPoint>& data);
    
    // Configuration management
    bool saveConfiguration(const std::string& config_file);
    bool loadDefaultConfiguration();
    void setOutputFormat(SBEnhanced::StatOutputFormat format);
    void setOutputFile(const std::string& filename);
    void setVerbose(bool verbose);
    
    // Error handling
    std::string getLastError() const;
    std::vector<std::string> getErrorLog() const;
    void clearErrorLog();
    
    // Performance metrics
    uint64_t getTotalCollections() const;
    uint64_t getSuccessfulCollections() const;
    uint64_t getFailedCollections() const;
    std::chrono::microseconds getAverageCollectionTime() const;
    
    // Web interface management
    bool startWebInterface(const SBEnhanced::WebServerConfig& config);
    bool stopWebInterface();
    bool isWebInterfaceRunning() const;
    bool restartWebInterface();
    std::string getWebInterfaceUrl() const;
    uint64_t getWebInterfaceRequests() const;
    std::string getWebInterfaceStatus() const;
    
private:
    // Internal helper methods
    void initializeAnalyzers();
    void initializeDefaultAlerts();
    void initializeDefaultReports();
    bool validateStatisticsOptions(const SBEnhanced::StatisticsOptions& options);
    bool validateDatabase();
    void logError(const std::string& error);
    void logMessage(const std::string& message);
    
    // Statistics collection helpers
    bool collectSystemStatistics();
    bool collectUserStatistics();
    bool collectMetadataStatistics();
    std::string buildStatisticsQuery(SBEnhanced::StatCategory category, const std::string& filter);
    
    // Analysis helpers
    SBEnhanced::HealthStatus determineHealthStatus(const SBEnhanced::PerformanceMetrics& metrics);
    std::vector<std::string> analyzeTableFragmentation(const std::vector<SBEnhanced::TableStatistics>& stats);
    std::vector<std::string> analyzeIndexUsage(const std::vector<SBEnhanced::IndexStatistics>& stats);
    std::vector<std::string> analyzeTransactionPatterns(const SBEnhanced::TransactionStatistics& stats);
    
    // Historical data helpers
    bool saveHistoricalData(const std::string& file_path);
    bool loadHistoricalData(const std::string& file_path);
    void aggregateHistoricalData(const std::string& metric_name, const std::string& period);
    
    // Monitoring helpers
    void processMetricsQueue();
    void sendAlert(const SBEnhanced::AlertConfig& alert, const std::string& message);
    bool evaluateAlertCondition(const SBEnhanced::AlertConfig& alert, double current_value);
    
    // Report generation helpers
    std::string generateHTMLReport(const SBEnhanced::ReportConfig& config);
    std::string generateCSVReport(const SBEnhanced::ReportConfig& config);
    std::string generateJSONReport(const SBEnhanced::ReportConfig& config);
    std::string generateMarkdownReport(const SBEnhanced::ReportConfig& config);
    
    // Utility helpers
    std::string escapeHTML(const std::string& text);
    std::string escapeCSV(const std::string& text);
    std::string escapeJSON(const std::string& text);
    std::vector<std::string> splitString(const std::string& str, char delimiter);
    std::string joinStrings(const std::vector<std::string>& strings, const std::string& delimiter);
    bool createDirectory(const std::string& path);
    bool fileExists(const std::string& path);
    uint64_t getFileSize(const std::string& path);
};
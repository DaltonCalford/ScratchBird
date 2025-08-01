#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>
#include <atomic>

// ScratchBird engine integration headers
#include "../jrd/Attachment.h"
#include "../jrd/tra.h"
#include "../jrd/svc.h"
#include "../jrd/SchemaPathCache.h"
#include "../dsql/DsqlStatements.h"
#include "../dsql/DsqlStatementCache.h"
#include "../jrd/trace/TraceManager.h"
#include "../jrd/Database.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/array.h"

namespace SBEnhanced {

// Forward declarations
class UtilityConfiguration;
class OutputFormatter;
class QueryAnalyzer;
class StatisticsCollector;

// Connection options
struct ConnectionOptions {
    std::string database_path;
    std::string username;
    std::string password;
    std::string role;
    std::string charset = "UTF8";
    int page_size = 16384;
    bool read_only = false;
    bool no_garbage_collect = false;
    std::map<std::string, std::string> dpb_options;
    std::chrono::seconds connect_timeout{30};
    std::chrono::seconds query_timeout{300};
    bool enable_tracing = false;
    bool enable_monitoring = true;
};

// Query execution results
struct QueryResults {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> column_names;
    std::vector<std::string> column_types;
    std::map<std::string, std::string> metadata;
    std::chrono::microseconds execution_time{0};
    uint64_t rows_affected = 0;
    uint64_t rows_fetched = 0;
    std::string plan_text;
    std::map<std::string, uint64_t> performance_counters;
    bool has_more_data = false;
    std::string error_message;
};

// DDL extraction types
enum class DDLType {
    DATABASE,
    TABLE,
    VIEW,
    PROCEDURE,
    FUNCTION,
    TRIGGER,
    DOMAIN,
    EXCEPTION,
    GENERATOR,
    ROLE,
    INDEX,
    CONSTRAINT,
    SCHEMA,
    PACKAGE,
    DATABASE_LINK,
    ALL
};

// Database statistics
struct DatabaseStatistics {
    // Database-level statistics
    uint64_t database_size_bytes = 0;
    uint64_t page_size = 0;
    uint64_t page_count = 0;
    uint64_t allocated_pages = 0;
    uint64_t used_pages = 0;
    uint64_t free_pages = 0;
    double fragmentation_ratio = 0.0;
    
    // Transaction statistics
    uint64_t oldest_transaction = 0;
    uint64_t oldest_active_transaction = 0;
    uint64_t oldest_snapshot = 0;
    uint64_t next_transaction = 0;
    uint64_t transaction_gap = 0;
    
    // Connection statistics
    uint64_t active_connections = 0;
    uint64_t peak_connections = 0;
    uint64_t total_connections = 0;
    
    // Performance metrics
    uint64_t page_reads = 0;
    uint64_t page_writes = 0;
    uint64_t cache_hits = 0;
    uint64_t cache_misses = 0;
    double cache_hit_ratio = 0.0;
    
    // Object counts
    uint64_t table_count = 0;
    uint64_t view_count = 0;
    uint64_t procedure_count = 0;
    uint64_t function_count = 0;
    uint64_t trigger_count = 0;
    uint64_t index_count = 0;
    uint64_t constraint_count = 0;
    uint64_t schema_count = 0;
    
    // Schema hierarchy statistics
    uint64_t max_schema_depth = 0;
    uint64_t total_hierarchical_schemas = 0;
    std::map<std::string, uint64_t> schema_object_counts;
    
    // Timing information
    std::chrono::steady_clock::time_point collection_time;
    std::chrono::microseconds collection_duration{0};
    
    // Error information
    std::vector<std::string> collection_errors;
    std::vector<std::string> collection_warnings;
};

// Backup/restore options
struct BackupOptions {
    std::string source_database;
    std::string backup_file;
    std::string metadata_file;
    bool include_data = true;
    bool include_metadata = true;
    bool include_system_tables = false;
    bool verbose = false;
    bool verify_backup = true;
    bool compress_backup = true;
    std::string compression_algorithm = "zstd";
    int compression_level = 3;
    bool parallel_processing = true;
    int worker_threads = 4;
    std::vector<std::string> exclude_tables;
    std::vector<std::string> include_tables;
    std::function<void(const std::string&)> progress_callback;
};

struct RestoreOptions {
    std::string backup_file;
    std::string target_database;
    std::string metadata_file;
    bool create_database = true;
    bool replace_database = false;
    bool restore_data = true;
    bool restore_metadata = true;
    bool verbose = false;
    bool verify_restore = true;
    bool parallel_processing = true;
    int worker_threads = 4;
    int page_size = 16384;
    std::vector<std::string> exclude_tables;
    std::vector<std::string> include_tables;
    std::function<void(const std::string&)> progress_callback;
};

// Performance metrics
struct PerformanceMetrics {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::microseconds total_execution_time{0};
    std::chrono::microseconds connection_time{0};
    std::chrono::microseconds query_preparation_time{0};
    std::chrono::microseconds query_execution_time{0};
    std::chrono::microseconds result_fetching_time{0};
    
    uint64_t queries_executed = 0;
    uint64_t queries_cached = 0;
    uint64_t cache_hits = 0;
    uint64_t cache_misses = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t rows_fetched = 0;
    uint64_t rows_affected = 0;
    
    std::map<std::string, uint64_t> custom_counters;
    std::map<std::string, std::chrono::microseconds> custom_timings;
    
    double getCacheHitRatio() const {
        return (cache_hits + cache_misses) > 0 ? 
               static_cast<double>(cache_hits) / (cache_hits + cache_misses) : 0.0;
    }
};

} // namespace SBEnhanced

// Main integration class
class SBEngineIntegration {
private:
    // Core ScratchBird components
    std::unique_ptr<Jrd::Attachment> attachment;
    std::unique_ptr<Jrd::Database> database;
    std::unique_ptr<Jrd::jrd_tra> transaction;
    std::unique_ptr<Jrd::Service> service;
    std::unique_ptr<Jrd::SchemaPathCache> schema_cache;
    std::unique_ptr<Jrd::DsqlStatementCache> statement_cache;
    std::unique_ptr<Jrd::TraceManager> trace_manager;
    
    // Configuration and state
    SBEnhanced::ConnectionOptions connection_options;
    SBEnhanced::PerformanceMetrics performance_metrics;
    std::atomic<bool> is_connected{false};
    std::atomic<bool> is_initialized{false};
    std::atomic<bool> monitoring_enabled{false};
    std::atomic<bool> tracing_enabled{false};
    
    // Enhanced utilities
    std::unique_ptr<SBEnhanced::UtilityConfiguration> config;
    std::unique_ptr<SBEnhanced::OutputFormatter> formatter;
    std::unique_ptr<SBEnhanced::QueryAnalyzer> analyzer;
    std::unique_ptr<SBEnhanced::StatisticsCollector> statistics_collector;
    
    // Connection management
    std::string current_database_path;
    std::string current_username;
    std::string current_role;
    std::chrono::steady_clock::time_point connection_start_time;
    
    // Error handling
    mutable std::vector<std::string> error_log;
    mutable std::atomic<uint64_t> error_count{0};
    std::string last_error;

public:
    SBEngineIntegration();
    ~SBEngineIntegration();
    
    // Initialization and cleanup
    bool initialize(const SBEnhanced::ConnectionOptions& options);
    bool shutdown();
    bool isInitialized() const { return is_initialized.load(); }
    bool isConnected() const { return is_connected.load(); }
    
    // Database connection management
    bool connectToDatabase(const std::string& db_path, const SBEnhanced::ConnectionOptions& options);
    bool disconnectFromDatabase();
    bool reconnectToDatabase();
    bool testConnection();
    
    // Transaction management
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    bool isInTransaction() const;
    
    // Query execution
    bool executeQuery(const std::string& sql, SBEnhanced::QueryResults& results);
    bool executeScript(const std::string& script_text, std::vector<SBEnhanced::QueryResults>& results);
    bool executePreparedQuery(const std::string& sql, const std::vector<std::string>& parameters, 
                             SBEnhanced::QueryResults& results);
    
    // DDL operations
    bool extractDDL(const std::string& object_name, SBEnhanced::DDLType type, std::string& ddl);
    bool extractDatabaseDDL(std::string& ddl);
    bool extractSchemaDDL(const std::string& schema_name, std::string& ddl);
    bool extractTableDDL(const std::string& table_name, std::string& ddl);
    bool extractViewDDL(const std::string& view_name, std::string& ddl);
    bool extractProcedureDDL(const std::string& procedure_name, std::string& ddl);
    
    // Statistics collection
    bool getStatistics(SBEnhanced::DatabaseStatistics& stats);
    bool getTableStatistics(const std::string& table_name, std::map<std::string, uint64_t>& stats);
    bool getIndexStatistics(const std::string& index_name, std::map<std::string, uint64_t>& stats);
    bool getSchemaStatistics(const std::string& schema_name, std::map<std::string, uint64_t>& stats);
    
    // Backup and restore operations
    bool performBackup(const SBEnhanced::BackupOptions& options);
    bool performRestore(const SBEnhanced::RestoreOptions& options);
    bool validateBackup(const std::string& backup_file);
    
    // Monitoring and tracing
    bool enableAdvancedMonitoring(bool enable);
    bool enableTracing(bool enable);
    SBEnhanced::PerformanceMetrics getPerformanceMetrics() const;
    std::vector<std::string> getOptimizationRecommendations();
    
    // Schema operations (leveraging existing hierarchical schema support)
    bool createSchema(const std::string& schema_name, const std::string& parent_schema = "");
    bool dropSchema(const std::string& schema_name, bool cascade = false);
    bool setCurrentSchema(const std::string& schema_name);
    std::string getCurrentSchema() const;
    std::vector<std::string> listSchemas(const std::string& pattern = "") const;
    bool validateSchemaHierarchy();
    
    // Configuration management
    bool loadConfiguration(const std::string& config_file);
    bool saveConfiguration(const std::string& config_file);
    bool setConfigOption(const std::string& key, const std::string& value);
    std::string getConfigOption(const std::string& key) const;
    
    // Error handling
    std::string getLastError() const;
    std::vector<std::string> getErrorLog() const;
    void clearErrorLog();
    uint64_t getErrorCount() const;
    
    // Utility methods
    std::string getEngineVersion() const;
    std::string getDatabaseVersion() const;
    std::map<std::string, std::string> getDatabaseProperties() const;
    std::vector<std::string> getActiveConnections() const;
    
    // Advanced features
    bool optimizeDatabase();
    bool validateDatabase();
    bool repairDatabase();
    bool analyzeStatistics();
    bool updateStatistics();
    
private:
    // Internal helper methods
    bool initializeEngineComponents();
    bool establishConnection();
    bool configureTracing();
    bool configureMonitoring();
    bool validateConnectionOptions(const SBEnhanced::ConnectionOptions& options);
    
    // Error handling helpers
    void logError(const std::string& operation, const std::string& error);
    void updatePerformanceMetrics(const std::string& operation, 
                                 std::chrono::microseconds duration);
    
    // Database operation helpers
    bool executeDDLStatement(const std::string& ddl);
    bool executeSystemQuery(const std::string& sql, SBEnhanced::QueryResults& results);
    bool buildDatabaseParameterBlock(const SBEnhanced::ConnectionOptions& options, 
                                   std::vector<uint8_t>& dpb);
    
    // Statistics collection helpers
    bool collectDatabaseLevelStats(SBEnhanced::DatabaseStatistics& stats);
    bool collectTransactionStats(SBEnhanced::DatabaseStatistics& stats);
    bool collectConnectionStats(SBEnhanced::DatabaseStatistics& stats);
    bool collectPerformanceStats(SBEnhanced::DatabaseStatistics& stats);
    bool collectObjectCounts(SBEnhanced::DatabaseStatistics& stats);
    bool collectSchemaHierarchyStats(SBEnhanced::DatabaseStatistics& stats);
    
    // Backup/restore helpers
    bool initializeBackupService(const SBEnhanced::BackupOptions& options);
    bool initializeRestoreService(const SBEnhanced::RestoreOptions& options);
    bool monitorServiceProgress(Jrd::Service* service, 
                              std::function<void(const std::string&)> progress_callback);
    
    // Schema operation helpers
    bool resolveSchemaPath(const std::string& schema_name, std::string& resolved_path);
    bool validateSchemaName(const std::string& schema_name);
    bool checkSchemaExists(const std::string& schema_name);
    bool checkSchemaHierarchyDepth(const std::string& schema_path);
    
    // Optimization helpers
    std::vector<std::string> analyzePerformanceBottlenecks();
    std::vector<std::string> analyzeIndexUsage();
    std::vector<std::string> analyzeTableFragmentation();
    std::vector<std::string> analyzeQueryPatterns();
    
    // Utility helpers
    std::string formatTimestamp(const std::chrono::steady_clock::time_point& time) const;
    std::string formatDuration(const std::chrono::microseconds& duration) const;
    std::string formatBytes(uint64_t bytes) const;
    std::string formatNumber(uint64_t number) const;
};
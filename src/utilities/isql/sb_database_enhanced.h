#pragma once

#include "sb_database.h"
#include <memory>
#include <vector>
#include <map>
#include <functional>
#include <chrono>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>

// Forward declarations
class AttachmentManager;
class TransactionManager;
class ServiceManager;
class MetadataCache;
class SchemaCache;
class QueryProcessor;
class ResultSetManager;
class MetadataExtractor;

// Enhanced data structures
namespace SBEnhanced {
    
    // DDL Types
    enum class DDLType {
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
        GRANT,
        PACKAGE,
        DATABASE
    };
    
    // Object Dependency
    struct ObjectDependency {
        std::string object_name;
        std::string object_type;
        std::string dependent_name;
        std::string dependent_type;
        std::string dependency_type;
        int dependency_level;
    };
    
    // System Statistics
    struct SystemStats {
        // Database statistics
        uint64_t database_size;
        uint32_t page_size;
        uint32_t page_count;
        uint32_t allocated_pages;
        uint32_t free_pages;
        std::string database_version;
        std::string creation_date;
        bool force_writes;
        bool read_only;
        
        // Connection statistics
        uint32_t active_connections;
        uint32_t total_connections;
        uint32_t peak_connections;
        
        // Transaction statistics
        uint64_t active_transactions;
        uint64_t committed_transactions;
        uint64_t rolled_back_transactions;
        uint64_t limbo_transactions;
        
        // Performance statistics
        uint64_t page_reads;
        uint64_t page_writes;
        uint64_t cache_hits;
        uint64_t cache_misses;
        double cache_hit_ratio;
        
        // Memory statistics
        uint64_t memory_used;
        uint64_t memory_allocated;
        uint64_t buffer_cache_size;
        uint64_t metadata_cache_size;
        
        // Schema statistics
        uint32_t schema_count;
        uint32_t table_count;
        uint32_t view_count;
        uint32_t procedure_count;
        uint32_t function_count;
        uint32_t trigger_count;
        uint32_t index_count;
        uint32_t constraint_count;
        uint32_t generator_count;
        uint32_t exception_count;
        uint32_t domain_count;
        uint32_t role_count;
        uint32_t user_count;
        uint32_t package_count;
    };
    
    // Execution Plan
    struct ExecutionPlan {
        std::string plan_text;
        std::vector<std::string> plan_steps;
        std::map<std::string, std::string> plan_attributes;
        double estimated_cost;
        uint64_t estimated_rows;
        std::chrono::milliseconds estimated_time;
        std::vector<std::string> used_indices;
        std::vector<std::string> accessed_tables;
        bool has_sort;
        bool has_join;
        bool has_aggregate;
        bool has_subquery;
    };
    
    // Query Analysis
    struct QueryAnalysis {
        std::string query_type;
        std::vector<std::string> referenced_tables;
        std::vector<std::string> referenced_columns;
        std::vector<std::string> referenced_procedures;
        std::vector<std::string> referenced_functions;
        std::vector<std::string> used_indices;
        std::vector<std::string> potential_indices;
        std::vector<std::string> warnings;
        std::vector<std::string> recommendations;
        bool is_complex;
        bool needs_optimization;
        double complexity_score;
    };
    
    // Backup/Restore Progress
    struct BackupProgress {
        std::atomic<uint64_t> total_objects{0};
        std::atomic<uint64_t> processed_objects{0};
        std::atomic<uint64_t> total_records{0};
        std::atomic<uint64_t> processed_records{0};
        std::atomic<uint64_t> total_bytes{0};
        std::atomic<uint64_t> processed_bytes{0};
        std::atomic<double> percentage{0.0};
        std::atomic<bool> completed{false};
        std::atomic<bool> cancelled{false};
        std::string current_operation;
        std::string current_object;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_update;
        std::function<void(const BackupProgress&)> callback;
    };
    
    struct RestoreProgress {
        std::atomic<uint64_t> total_objects{0};
        std::atomic<uint64_t> processed_objects{0};
        std::atomic<uint64_t> total_records{0};
        std::atomic<uint64_t> processed_records{0};
        std::atomic<uint64_t> total_bytes{0};
        std::atomic<uint64_t> processed_bytes{0};
        std::atomic<double> percentage{0.0};
        std::atomic<bool> completed{false};
        std::atomic<bool> cancelled{false};
        std::string current_operation;
        std::string current_object;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_update;
        std::function<void(const RestoreProgress&)> callback;
    };
    
    // Backup/Restore Options
    struct BackupOptions {
        std::string database_path;
        std::string backup_path;
        std::string username;
        std::string password;
        std::string role;
        bool trusted_auth = false;
        
        // Backup options
        bool metadata_only = false;
        bool transportable = false;
        bool no_garbage_collect = false;
        bool ignore_checksums = false;
        bool ignore_limbo = false;
        bool convert_external_tables = false;
        bool compress = false;
        bool encrypt = false;
        bool verbose = false;
        bool statistics = false;
        
        // Filtering options
        std::vector<std::string> include_tables;
        std::vector<std::string> exclude_tables;
        std::vector<std::string> include_schemas;
        std::vector<std::string> exclude_schemas;
        
        // Advanced options
        std::string compression_level = "6";
        std::string encryption_key;
        std::string encryption_algorithm = "AES-256";
        uint32_t parallel_workers = 1;
        uint32_t buffer_size = 1024 * 1024; // 1MB
        uint32_t commit_interval = 1000;
        
        // Progress tracking
        std::function<void(const BackupProgress&)> progress_callback;
        std::function<bool()> cancel_callback;
    };
    
    struct RestoreOptions {
        std::string backup_path;
        std::string database_path;
        std::string username;
        std::string password;
        std::string role;
        bool trusted_auth = false;
        
        // Restore options
        bool create_database = false;
        bool replace_database = false;
        bool deactivate_indexes = false;
        bool no_validity = false;
        bool one_at_a_time = false;
        bool use_all_space = false;
        bool metadata_only = false;
        bool data_only = false;
        bool kill_shadows = false;
        bool fix_fss_data = false;
        bool fix_fss_metadata = false;
        bool verbose = false;
        bool statistics = false;
        
        // Database creation options
        uint32_t page_size = 8192;
        uint32_t page_buffers = 256;
        std::string character_set = "UTF8";
        std::string collation = "UTF8";
        bool force_writes = true;
        
        // Advanced options
        uint32_t parallel_workers = 1;
        uint32_t buffer_size = 1024 * 1024; // 1MB
        uint32_t commit_interval = 1000;
        
        // Progress tracking
        std::function<void(const RestoreProgress&)> progress_callback;
        std::function<bool()> cancel_callback;
    };
    
    // Page Analysis
    struct PageAnalysisOptions {
        std::string table_name;
        std::string index_name;
        std::string schema_name;
        bool analyze_data_pages = true;
        bool analyze_index_pages = true;
        bool analyze_blob_pages = true;
        bool analyze_fill_factors = true;
        bool analyze_fragmentation = true;
        bool detailed_analysis = false;
        uint32_t sample_size = 1000;
    };
    
    struct PageStatistics {
        uint64_t total_pages;
        uint64_t data_pages;
        uint64_t index_pages;
        uint64_t blob_pages;
        uint64_t free_pages;
        uint64_t empty_pages;
        double average_fill_factor;
        double fragmentation_ratio;
        std::map<std::string, uint64_t> page_type_counts;
        std::map<std::string, double> fill_factor_distribution;
        std::vector<std::string> recommendations;
    };
    
} // namespace SBEnhanced

// Enhanced Database Class
class SBDatabaseEnhanced : public SBDatabase {
private:
    // Enhanced connection management
    std::unique_ptr<AttachmentManager> attachment_mgr;
    std::unique_ptr<TransactionManager> transaction_mgr;
    std::unique_ptr<ServiceManager> service_mgr;
    
    // Metadata caching
    std::unique_ptr<MetadataCache> metadata_cache;
    std::unique_ptr<SchemaCache> schema_cache;
    
    // Advanced query processing
    std::unique_ptr<QueryProcessor> query_processor;
    std::unique_ptr<ResultSetManager> result_mgr;
    
    // Thread safety
    mutable std::recursive_mutex connection_mutex;
    mutable std::shared_mutex cache_mutex;
    
    // Configuration
    std::map<std::string, std::string> config_options;
    
    // Statistics
    mutable SBEnhanced::SystemStats cached_stats;
    mutable std::chrono::steady_clock::time_point stats_cache_time;
    mutable std::chrono::seconds stats_cache_duration{30};
    
public:
    SBDatabaseEnhanced();
    virtual ~SBDatabaseEnhanced();
    
    // Enhanced connection management
    bool connectEnhanced(const std::string& db_name, const std::string& user, 
                        const std::string& pass, const std::string& role, 
                        bool trusted, const std::map<std::string, std::string>& options = {});
    bool disconnectEnhanced();
    bool isConnectedEnhanced() const;
    
    // Configuration
    bool setConfigOption(const std::string& key, const std::string& value);
    std::string getConfigOption(const std::string& key) const;
    std::map<std::string, std::string> getAllConfigOptions() const;
    
    // Enhanced metadata operations
    bool extractDDL(const std::string& object_name, SBEnhanced::DDLType type, std::string& ddl);
    bool extractAllDDL(std::ostream& output, const std::vector<SBEnhanced::DDLType>& types = {});
    bool getObjectDependencies(const std::string& object_name, std::vector<SBEnhanced::ObjectDependency>& deps);
    bool getSystemStatistics(SBEnhanced::SystemStats& stats);
    bool refreshStatistics();
    
    // Advanced query operations
    bool executeWithPlan(const std::string& sql, SBEnhanced::ExecutionPlan& plan);
    bool analyzeQuery(const std::string& sql, SBEnhanced::QueryAnalysis& analysis);
    bool validateSQL(const std::string& sql, std::vector<std::string>& errors);
    bool getQueryPerformance(const std::string& sql, std::map<std::string, double>& metrics);
    
    // Enhanced result processing
    bool executeSelectEnhanced(const std::string& sql, 
                             std::vector<std::vector<std::string>>& results,
                             std::vector<std::string>& column_names,
                             std::vector<std::string>& column_types);
    bool executeSelectWithCallback(const std::string& sql, 
                                  std::function<bool(const std::vector<std::string>&)> callback);
    bool executeSelectStreaming(const std::string& sql, 
                               std::function<bool(const std::vector<std::string>&)> callback);
    
    // Backup/Restore operations
    bool createBackup(const SBEnhanced::BackupOptions& options, SBEnhanced::BackupProgress& progress);
    bool restoreBackup(const SBEnhanced::RestoreOptions& options, SBEnhanced::RestoreProgress& progress);
    bool validateBackup(const std::string& backup_path, std::vector<std::string>& errors);
    
    // Page-level analysis
    bool analyzePages(const SBEnhanced::PageAnalysisOptions& options, SBEnhanced::PageStatistics& stats);
    bool getPageDetails(uint32_t page_number, std::map<std::string, std::string>& details);
    bool optimizeDatabase(const std::vector<std::string>& options = {});
    
    // Cache management
    bool clearMetadataCache();
    bool clearSchemaCache();
    bool clearAllCaches();
    bool preloadCache(const std::vector<std::string>& objects = {});
    
    // Advanced transaction management
    bool beginTransaction(const std::string& name = "");
    bool commitTransaction(const std::string& name = "");
    bool rollbackTransaction(const std::string& name = "");
    bool savepoint(const std::string& name);
    bool rollbackToSavepoint(const std::string& name);
    bool releaseSavepoint(const std::string& name);
    
    // Schema management
    bool setCurrentSchema(const std::string& schema_name);
    std::string getCurrentSchema() const;
    bool setSearchPath(const std::vector<std::string>& schemas);
    std::vector<std::string> getSearchPath() const;
    bool resolveObjectName(const std::string& object_name, std::string& resolved_name);
    
    // Performance monitoring
    bool enablePerformanceMonitoring(bool enable = true);
    bool getPerformanceCounters(std::map<std::string, uint64_t>& counters);
    bool resetPerformanceCounters();
    
    // Utility methods
    std::string formatError(const std::string& context = "") const;
    bool testConnection() const;
    std::string getServerVersion() const;
    std::string getClientVersion() const;
    std::map<std::string, std::string> getConnectionInfo() const;
    
    // Manager access (for specialized operations)
    AttachmentManager* getAttachmentManager() const { return attachment_mgr.get(); }
    TransactionManager* getTransactionManager() const { return transaction_mgr.get(); }
    ServiceManager* getServiceManager() const { return service_mgr.get(); }
    MetadataCache* getMetadataCache() const { return metadata_cache.get(); }
    SchemaCache* getSchemaCache() const { return schema_cache.get(); }
    QueryProcessor* getQueryProcessor() const { return query_processor.get(); }
    ResultSetManager* getResultSetManager() const { return result_mgr.get(); }
    
private:
    // Internal initialization
    bool initializeManagers();
    bool shutdownManagers();
    
    // Internal helpers
    bool updateSystemStats(SBEnhanced::SystemStats& stats) const;
    bool isStatsCacheValid() const;
    void logError(const std::string& operation, const std::string& error) const;
    void logPerformance(const std::string& operation, std::chrono::microseconds duration) const;
};
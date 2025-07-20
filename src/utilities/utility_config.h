#pragma once

#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

// Forward declarations
namespace SBEnhanced {
    enum class OutputFormat;
    enum class ExportFormat;
    enum class CompressionLevel;
}

namespace SBEnhanced {

// Compression levels
enum class CompressionLevel {
    NONE = 0,
    FASTEST = 1,
    FAST = 2,
    MEDIUM = 3,
    HIGH = 4,
    HIGHEST = 5
};

// Connection pooling configuration
struct ConnectionPoolConfig {
    int min_connections = 1;
    int max_connections = 10;
    int initial_connections = 2;
    std::chrono::seconds connection_timeout{30};
    std::chrono::seconds idle_timeout{300};
    std::chrono::seconds max_lifetime{3600};
    bool validate_connections = true;
    bool retry_failed_connections = true;
    int max_retry_attempts = 3;
    std::chrono::seconds retry_delay{5};
    bool enable_connection_pooling = true;
    bool enable_connection_monitoring = true;
};

// Caching configuration
struct CacheConfig {
    bool enable_query_cache = true;
    bool enable_metadata_cache = true;
    bool enable_result_cache = false;
    std::size_t max_cache_size_mb = 100;
    std::chrono::seconds cache_ttl{3600};
    std::chrono::seconds cleanup_interval{300};
    double eviction_threshold = 0.8;
    bool enable_cache_statistics = true;
    bool enable_cache_persistence = false;
    std::string cache_directory = "./cache";
};

// Logging configuration
struct LoggingConfig {
    bool enable_logging = true;
    bool enable_debug_logging = false;
    bool enable_performance_logging = true;
    bool enable_error_logging = true;
    bool enable_audit_logging = false;
    std::string log_directory = "./logs";
    std::string log_file_prefix = "sb_utility";
    std::chrono::seconds log_rotation_interval{86400}; // 24 hours
    std::size_t max_log_file_size_mb = 10;
    int max_log_files = 7;
    bool log_to_console = true;
    bool log_to_file = true;
    bool log_with_timestamps = true;
    bool log_with_thread_id = true;
};

// Security configuration
struct SecurityConfig {
    bool enable_ssl = false;
    std::string ssl_cert_file;
    std::string ssl_key_file;
    std::string ssl_ca_file;
    bool verify_ssl_certificates = true;
    bool enable_encryption = false;
    std::string encryption_key;
    std::string encryption_algorithm = "AES256";
    bool enable_authentication = true;
    std::string auth_method = "NATIVE";
    bool enable_authorization = true;
    std::chrono::seconds session_timeout{3600};
    int max_failed_login_attempts = 3;
    std::chrono::seconds lockout_duration{300};
    bool enable_audit_trail = false;
};

// Performance configuration
struct PerformanceConfig {
    bool enable_performance_monitoring = true;
    bool enable_query_profiling = true;
    bool enable_connection_monitoring = true;
    bool enable_resource_monitoring = true;
    std::chrono::seconds monitoring_interval{30};
    std::chrono::seconds profiling_interval{60};
    std::chrono::microseconds slow_query_threshold{1000000}; // 1 second
    bool enable_optimization_hints = true;
    bool enable_auto_optimization = false;
    bool enable_statistics_collection = true;
    std::chrono::seconds statistics_collection_interval{300};
    bool enable_trend_analysis = true;
    int max_performance_samples = 1000;
};

// Backup configuration
struct BackupConfig {
    std::string default_backup_directory = "./backups";
    CompressionLevel default_compression = CompressionLevel::MEDIUM;
    std::string default_compression_algorithm = "zstd";
    bool default_verify_backup = true;
    bool default_include_metadata = true;
    bool default_include_data = true;
    bool default_include_system_tables = false;
    bool default_parallel_processing = true;
    int default_worker_threads = 4;
    std::chrono::seconds backup_timeout{3600};
    bool enable_incremental_backup = false;
    bool enable_differential_backup = false;
    std::string backup_file_extension = ".sbk";
    std::string metadata_file_extension = ".smd";
    bool enable_backup_encryption = false;
    std::string backup_encryption_key;
    bool enable_backup_checksums = true;
    bool enable_backup_logging = true;
};

} // namespace SBEnhanced

// Main configuration class
class UtilityConfiguration {
private:
    std::map<std::string, std::string> config_options;
    std::string config_file_path;
    bool config_loaded = false;
    std::chrono::steady_clock::time_point last_modified_time;
    
    // Configuration sections
    SBEnhanced::ConnectionPoolConfig connection_pool_config;
    SBEnhanced::CacheConfig cache_config;
    SBEnhanced::LoggingConfig logging_config;
    SBEnhanced::SecurityConfig security_config;
    SBEnhanced::PerformanceConfig performance_config;
    SBEnhanced::BackupConfig backup_config;
    
    // Utility-specific configurations
    struct ISQLConfig {
        SBEnhanced::OutputFormat default_format = SBEnhanced::OutputFormat::TABLE;
        int page_size = 20;
        bool show_headers = true;
        bool show_row_numbers = false;
        bool show_statistics = false;
        bool show_query_time = true;
        bool show_row_count = true;
        bool enable_history = true;
        std::string history_file = "~/.sb_isql_history";
        int max_history_size = 1000;
        bool enable_auto_commit = true;
        bool enable_echo_commands = false;
        bool enable_result_paging = true;
        bool enable_syntax_highlighting = true;
        bool enable_auto_completion = true;
        std::string prompt = "SB> ";
        std::string continuation_prompt = "CON> ";
        int max_column_width = 50;
        std::string null_display = "NULL";
        std::string date_format = "%Y-%m-%d %H:%M:%S";
        bool enable_transaction_warnings = true;
        bool enable_error_details = true;
        std::vector<std::string> startup_commands;
        std::vector<std::string> shutdown_commands;
        std::map<std::string, std::string> custom_commands;
    } isql_config;
    
    struct GStatConfig {
        bool detailed_analysis = false;
        bool include_system_tables = false;
        bool include_schema_statistics = true;
        bool include_index_statistics = true;
        bool include_table_statistics = true;
        bool include_performance_metrics = true;
        bool include_fragmentation_analysis = true;
        bool include_trend_analysis = false;
        bool include_optimization_recommendations = true;
        SBEnhanced::ExportFormat default_export_format = SBEnhanced::ExportFormat::TEXT;
        std::string export_directory = "./stats";
        std::string report_template = "default";
        bool enable_report_generation = true;
        bool enable_comparison_reports = false;
        bool enable_historical_tracking = false;
        std::chrono::seconds collection_interval{300};
        bool enable_auto_refresh = true;
        bool enable_real_time_monitoring = false;
        int max_report_history = 30;
        bool enable_email_reports = false;
        std::string email_recipients;
        std::string email_server;
        std::string email_subject_prefix = "[ScratchBird Stats]";
        std::vector<std::string> custom_queries;
        std::map<std::string, std::string> custom_metrics;
    } gstat_config;
    
    struct GBakConfig {
        std::string default_backup_directory = "./backups";
        SBEnhanced::CompressionLevel default_compression = SBEnhanced::CompressionLevel::MEDIUM;
        std::string default_compression_algorithm = "zstd";
        bool default_verify_backup = true;
        bool default_include_metadata = true;
        bool default_include_data = true;
        bool default_include_system_tables = false;
        bool default_parallel_processing = true;
        int default_worker_threads = 4;
        bool enable_incremental_backup = false;
        bool enable_differential_backup = false;
        bool enable_backup_encryption = false;
        std::string backup_encryption_key;
        bool enable_backup_checksums = true;
        bool enable_backup_validation = true;
        bool enable_restore_validation = true;
        bool enable_progress_monitoring = true;
        bool enable_email_notifications = false;
        std::string notification_email;
        std::string notification_smtp_server;
        std::chrono::seconds backup_timeout{3600};
        std::chrono::seconds restore_timeout{3600};
        bool enable_backup_scheduling = false;
        std::string backup_schedule_cron;
        std::string backup_retention_policy = "30d";
        bool enable_backup_rotation = true;
        int max_backup_files = 10;
        bool enable_backup_logging = true;
        std::string backup_log_directory = "./logs/backup";
        std::vector<std::string> pre_backup_commands;
        std::vector<std::string> post_backup_commands;
        std::vector<std::string> pre_restore_commands;
        std::vector<std::string> post_restore_commands;
        std::map<std::string, std::string> custom_options;
    } gbak_config;
    
public:
    UtilityConfiguration();
    ~UtilityConfiguration() = default;
    
    // Configuration file management
    bool loadConfiguration(const std::string& config_file);
    bool saveConfiguration(const std::string& config_file);
    bool reloadConfiguration();
    bool isConfigurationLoaded() const { return config_loaded; }
    std::string getConfigurationFile() const { return config_file_path; }
    
    // General option management
    bool setOption(const std::string& key, const std::string& value);
    std::string getOption(const std::string& key) const;
    bool hasOption(const std::string& key) const;
    bool removeOption(const std::string& key);
    std::map<std::string, std::string> getAllOptions() const;
    
    // Configuration section accessors
    const SBEnhanced::ConnectionPoolConfig& getConnectionPoolConfig() const { return connection_pool_config; }
    const SBEnhanced::CacheConfig& getCacheConfig() const { return cache_config; }
    const SBEnhanced::LoggingConfig& getLoggingConfig() const { return logging_config; }
    const SBEnhanced::SecurityConfig& getSecurityConfig() const { return security_config; }
    const SBEnhanced::PerformanceConfig& getPerformanceConfig() const { return performance_config; }
    const SBEnhanced::BackupConfig& getBackupConfig() const { return backup_config; }
    
    // Configuration section setters
    void setConnectionPoolConfig(const SBEnhanced::ConnectionPoolConfig& config) { connection_pool_config = config; }
    void setCacheConfig(const SBEnhanced::CacheConfig& config) { cache_config = config; }
    void setLoggingConfig(const SBEnhanced::LoggingConfig& config) { logging_config = config; }
    void setSecurityConfig(const SBEnhanced::SecurityConfig& config) { security_config = config; }
    void setPerformanceConfig(const SBEnhanced::PerformanceConfig& config) { performance_config = config; }
    void setBackupConfig(const SBEnhanced::BackupConfig& config) { backup_config = config; }
    
    // Utility-specific configuration accessors
    const decltype(isql_config)& getISQLConfig() const { return isql_config; }
    const decltype(gstat_config)& getGStatConfig() const { return gstat_config; }
    const decltype(gbak_config)& getGBakConfig() const { return gbak_config; }
    
    // Utility-specific configuration setters
    void setISQLConfig(const decltype(isql_config)& config) { isql_config = config; }
    void setGStatConfig(const decltype(gstat_config)& config) { gstat_config = config; }
    void setGBakConfig(const decltype(gbak_config)& config) { gbak_config = config; }
    
    // Configuration validation
    bool validateConfiguration();
    std::vector<std::string> getConfigurationErrors();
    std::vector<std::string> getConfigurationWarnings();
    
    // Default configuration
    void loadDefaultConfiguration();
    void resetToDefaults();
    
    // Configuration templates
    bool loadConfigurationTemplate(const std::string& template_name);
    bool saveConfigurationTemplate(const std::string& template_name);
    std::vector<std::string> getAvailableTemplates();
    
    // Environment variable support
    bool loadFromEnvironment(const std::string& prefix = "SB_");
    std::map<std::string, std::string> getEnvironmentVariables(const std::string& prefix = "SB_");
    
    // Configuration merging
    bool mergeConfiguration(const UtilityConfiguration& other);
    bool mergeFromFile(const std::string& config_file);
    
    // Configuration export/import
    bool exportToJSON(const std::string& filename);
    bool importFromJSON(const std::string& filename);
    bool exportToXML(const std::string& filename);
    bool importFromXML(const std::string& filename);
    
    // Configuration monitoring
    bool isConfigurationChanged();
    std::chrono::steady_clock::time_point getLastModifiedTime() const { return last_modified_time; }
    void updateLastModifiedTime();
    
    // Configuration backup/restore
    bool backupConfiguration(const std::string& backup_file);
    bool restoreConfiguration(const std::string& backup_file);
    
    // Utility methods
    std::string toString() const;
    std::string toJSON() const;
    std::string toXML() const;
    
private:
    // Internal configuration management
    bool parseConfigurationFile(const std::string& filename);
    bool writeConfigurationFile(const std::string& filename);
    bool parseConfigurationSection(const std::string& section, const std::string& content);
    
    // Type conversion helpers
    template<typename T>
    T convertValue(const std::string& value);
    
    template<typename T>
    std::string convertToString(const T& value);
    
    // Configuration validation helpers
    bool validateConnectionPoolConfig();
    bool validateCacheConfig();
    bool validateLoggingConfig();
    bool validateSecurityConfig();
    bool validatePerformanceConfig();
    bool validateBackupConfig();
    bool validateUtilityConfigs();
    
    // Default value helpers
    void setDefaultConnectionPoolConfig();
    void setDefaultCacheConfig();
    void setDefaultLoggingConfig();
    void setDefaultSecurityConfig();
    void setDefaultPerformanceConfig();
    void setDefaultBackupConfig();
    void setDefaultUtilityConfigs();
    
    // File I/O helpers
    bool fileExists(const std::string& filename);
    bool createDirectory(const std::string& directory);
    std::string getFileExtension(const std::string& filename);
    std::string getAbsolutePath(const std::string& relative_path);
    
    // String manipulation helpers
    std::string trim(const std::string& str);
    std::string toLower(const std::string& str);
    std::string toUpper(const std::string& str);
    std::vector<std::string> split(const std::string& str, const std::string& delimiter);
    std::string join(const std::vector<std::string>& parts, const std::string& delimiter);
    
    // Configuration format helpers
    std::string formatConfigSection(const std::string& section_name, const std::map<std::string, std::string>& options);
    std::map<std::string, std::string> parseConfigSection(const std::string& section_content);
    
    // Error handling
    mutable std::vector<std::string> error_messages;
    mutable std::vector<std::string> warning_messages;
    void addError(const std::string& error) const;
    void addWarning(const std::string& warning) const;
    void clearMessages() const;
};
#pragma once

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/provider_dispatch.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace scratchbird::engine
{
    /// Forward declarations
    class EmbeddedProvider;
    class RemoteProvider;

    /// Provider configuration parameters
    struct ProviderConfig {
        std::string provider_name;
        std::string provider_version = "1.0.0";
        ProviderType provider_type;
        ProviderCapabilities capabilities;

        // Performance settings
        std::uint32_t connection_pool_size = 100;
        std::uint32_t max_concurrent_transactions = 1000;
        std::uint32_t statement_cache_size = 1000;
        std::uint32_t timeout_ms = 30000;

        // Resource limits
        std::uint64_t max_memory_mb = 1024;
        std::uint32_t max_open_databases = 50;
        std::uint32_t max_prepared_statements = 10000;

        // Provider-specific options
        std::map<std::string, std::string> custom_options;

        // Validation
        bool validate() const;
        std::string to_string() const;
    };

    /// Provider factory registry
    class DatabaseProviderFactory
    {
      public:
        static DatabaseProviderFactory& instance();

        // Factory registration
        void register_embedded_provider();
        void register_remote_provider();
        void register_legacy_provider();
        void
        register_third_party_provider(const std::string& name,
                                      std::function<std::unique_ptr<DatabaseProvider>()> factory);

        // Provider creation
        std::unique_ptr<DatabaseProvider> create_provider(ProviderType type,
                                                          const ProviderConfig& config);
        std::unique_ptr<DatabaseProvider> create_provider(const std::string& provider_name,
                                                          const ProviderConfig& config);

        // Registry management
        std::vector<std::string> get_registered_providers() const;
        bool is_provider_registered(const std::string& provider_name) const;
        ProviderCapabilities get_provider_capabilities(const std::string& provider_name) const;

      private:
        DatabaseProviderFactory() = default;
        std::map<std::string,
                 std::function<std::unique_ptr<DatabaseProvider>(const ProviderConfig&)>>
            factories_;
        std::map<std::string, ProviderCapabilities> capabilities_registry_;
        mutable std::mutex factory_mutex_;
    };

    /// Simple provider base class
    class SimpleProvider : public DatabaseProvider
    {
      public:
        SimpleProvider(const ProviderConfig& config, ProviderType type)
            : config_(config), type_(type), initialized_(false)
        {
        }
        virtual ~SimpleProvider() = default;

        ProviderType get_provider_type() const override
        {
            return type_;
        }
        std::string get_provider_name() const override
        {
            return config_.provider_name;
        }
        std::string get_provider_version() const override
        {
            return config_.provider_version;
        }
        ProviderCapabilities get_capabilities() const override
        {
            return config_.capabilities;
        }

        bool initialize() override
        {
            initialized_ = true;
            return true;
        }
        void shutdown() override
        {
            initialized_ = false;
        }
        bool is_initialized() const override
        {
            return initialized_;
        }

        bool can_handle_connection(const ConnectionInfo& conn_info) const override
        {
            return conn_info.get_provider_type() == type_;
        }

        std::unique_ptr<DatabaseOperations> create_database_operations() override;
        std::unique_ptr<TransactionOperations> create_transaction_operations() override;
        std::unique_ptr<StatementOperations> create_statement_operations() override;
        std::unique_ptr<SecurityOperations> create_security_operations() override;

        void cleanup_resources() override {}
        std::uint32_t get_active_connections() const override
        {
            return active_connections_;
        }
        ProviderStats get_statistics() const override
        {
            return statistics_;
        }
        std::string get_last_error() const override
        {
            return last_error_;
        }

        // Public methods for operations to update statistics
        void increment_active_connections()
        {
            active_connections_++;
            statistics_.connections_created++;
            statistics_.connections_active++;
        }
        void decrement_active_connections()
        {
            if (active_connections_ > 0) {
                active_connections_--;
                statistics_.connections_active--;
            }
        }

      protected:
        ProviderConfig config_;
        ProviderType type_;
        std::atomic<bool> initialized_;
        std::atomic<std::uint32_t> active_connections_{0};
        ProviderStats statistics_;
        std::string last_error_;
    };

    /// Embedded database provider implementation
    class EmbeddedProvider : public SimpleProvider
    {
      public:
        explicit EmbeddedProvider(const ProviderConfig& config);
        ~EmbeddedProvider() override = default;
    };

    /// Remote database provider implementation
    class RemoteProvider : public SimpleProvider
    {
      public:
        explicit RemoteProvider(const ProviderConfig& config);
        ~RemoteProvider() override = default;
    };

    /// Legacy provider for backward compatibility
    class LegacyProvider : public SimpleProvider
    {
      public:
        explicit LegacyProvider(const ProviderConfig& config);
        ~LegacyProvider() override = default;
    };

    /// Provider performance monitor
    class ProviderPerformanceMonitor
    {
      public:
        explicit ProviderPerformanceMonitor(DatabaseProvider* provider);
        ~ProviderPerformanceMonitor();

        // Performance tracking
        void start_monitoring();
        void stop_monitoring();
        bool is_monitoring() const;

        // Metrics collection
        struct PerformanceMetrics {
            std::uint64_t connections_per_second = 0;
            std::uint64_t queries_per_second = 0;
            std::uint64_t transactions_per_second = 0;
            std::uint64_t avg_response_time_ms = 0;
            std::uint64_t memory_usage_mb = 0;
            std::uint64_t cpu_usage_percent = 0;

            std::map<std::string, std::uint64_t> custom_metrics;
        };

        PerformanceMetrics get_current_metrics() const;
        PerformanceMetrics get_average_metrics(std::chrono::seconds window) const;
        void reset_metrics();

        // Alerts and thresholds
        void set_alert_threshold(const std::string& metric_name, std::uint64_t threshold);
        void remove_alert_threshold(const std::string& metric_name);
        std::vector<std::string> get_active_alerts() const;

      private:
        DatabaseProvider* provider_;
        std::atomic<bool> monitoring_enabled_;
        std::thread monitoring_thread_;
        mutable std::mutex metrics_mutex_;

        std::map<std::string, std::uint64_t> alert_thresholds_;
        std::vector<std::string> active_alerts_;

        // Metrics storage
        struct MetricsEntry {
            std::chrono::steady_clock::time_point timestamp;
            PerformanceMetrics metrics;
        };
        std::deque<MetricsEntry> metrics_history_;
        static constexpr std::size_t MAX_HISTORY_SIZE = 3600; // 1 hour at 1 sample/second

        void monitoring_worker();
        void collect_metrics();
        void check_alert_thresholds();
    };

    /// Provider resource manager
    class ProviderResourceManager
    {
      public:
        explicit ProviderResourceManager(const ProviderConfig& config);
        ~ProviderResourceManager();

        // Resource allocation
        bool allocate_connection_slot();
        void release_connection_slot();
        bool allocate_transaction_slot();
        void release_transaction_slot();
        bool allocate_statement_slot();
        void release_statement_slot();

        // Memory management
        bool allocate_memory(std::size_t bytes);
        void release_memory(std::size_t bytes);
        std::size_t get_allocated_memory() const;
        std::size_t get_available_memory() const;

        // Resource limits
        bool is_connection_limit_reached() const;
        bool is_transaction_limit_reached() const;
        bool is_statement_limit_reached() const;
        bool is_memory_limit_reached() const;

        // Resource statistics
        struct ResourceStats {
            std::uint32_t active_connections = 0;
            std::uint32_t max_connections = 0;
            std::uint32_t active_transactions = 0;
            std::uint32_t max_transactions = 0;
            std::uint32_t active_statements = 0;
            std::uint32_t max_statements = 0;
            std::uint64_t allocated_memory_mb = 0;
            std::uint64_t max_memory_mb = 0;
        };

        ResourceStats get_resource_stats() const;
        void reset_peak_usage();

      private:
        ProviderConfig config_;
        mutable std::mutex resource_mutex_;

        std::atomic<std::uint32_t> active_connections_;
        std::atomic<std::uint32_t> active_transactions_;
        std::atomic<std::uint32_t> active_statements_;
        std::atomic<std::uint64_t> allocated_memory_bytes_;

        // Peak usage tracking
        std::atomic<std::uint32_t> peak_connections_;
        std::atomic<std::uint32_t> peak_transactions_;
        std::atomic<std::uint32_t> peak_statements_;
        std::atomic<std::uint64_t> peak_memory_bytes_;
    };

    /// Provider error handler
    class ProviderErrorHandler
    {
      public:
        enum class ErrorSeverity { Info, Warning, Error, Fatal };

        enum class ErrorCategory {
            Connection,
            Authentication,
            Authorization,
            Transaction,
            Statement,
            Resource,
            Network,
            Internal
        };

        struct ErrorInfo {
            std::int32_t error_code;
            std::string error_message;
            ErrorSeverity severity;
            ErrorCategory category;
            std::chrono::steady_clock::time_point timestamp;
            std::string context;
        };

        // Error logging
        void log_error(std::int32_t code, const std::string& message, ErrorSeverity severity,
                       ErrorCategory category, const std::string& context = "");

        void log_info(const std::string& message, const std::string& context = "");
        void log_warning(const std::string& message, const std::string& context = "");
        void log_error(const std::string& message, const std::string& context = "");
        void log_fatal(const std::string& message, const std::string& context = "");

        // Error retrieval
        std::string get_last_error() const;
        std::int32_t get_last_error_code() const;
        std::vector<ErrorInfo>
        get_recent_errors(std::chrono::seconds window = std::chrono::seconds(300)) const;
        std::vector<ErrorInfo> get_errors_by_category(ErrorCategory category) const;
        std::vector<ErrorInfo> get_errors_by_severity(ErrorSeverity severity) const;

        // Error statistics
        std::uint64_t get_error_count() const;
        std::uint64_t get_error_count_by_category(ErrorCategory category) const;
        std::uint64_t get_error_count_by_severity(ErrorSeverity severity) const;
        void reset_error_counts();

        // Error handling policies
        void set_max_error_history(std::size_t max_size);
        void set_auto_cleanup_interval(std::chrono::seconds interval);
        void enable_error_callbacks(bool enabled);

      private:
        mutable std::mutex error_mutex_;
        std::deque<ErrorInfo> error_history_;
        std::size_t max_error_history_ = 10000;
        std::atomic<std::uint64_t> total_error_count_;
        std::map<ErrorCategory, std::uint64_t> category_counts_;
        std::map<ErrorSeverity, std::uint64_t> severity_counts_;

        void cleanup_old_errors();
    };

} // namespace scratchbird::engine

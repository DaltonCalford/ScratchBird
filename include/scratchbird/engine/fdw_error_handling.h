#pragma once

#include "scratchbird/engine/types.h"

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    /// FDW error categories for structured error handling
    enum class FdwErrorCategory {
        Connection,     // Connection establishment and management errors
        Authentication, // Authentication and authorization failures
        Network,        // Network-related errors (timeouts, disconnects)
        Query,          // Query execution errors
        DataType,       // Data type conversion errors
        Transaction,    // Transaction management errors
        Configuration,  // Configuration and setup errors
        Resource,       // Resource exhaustion (memory, connections)
        Security,       // Security policy violations
        Internal,       // Internal FDW implementation errors
        Unknown         // Catch-all for unclassified errors
    };

    /// FDW error severity levels
    enum class FdwErrorSeverity {
        Info,     // Informational messages
        Warning,  // Warning conditions
        Error,    // Error conditions that prevent operation
        Critical, // Critical errors requiring immediate attention
        Fatal     // Fatal errors that require connection termination
    };

    /// Recovery action recommendations for error conditions
    enum class FdwRecoveryAction {
        None,           // No recovery action needed
        Retry,          // Retry the operation
        RetryWithDelay, // Retry after a delay
        Reconnect,      // Re-establish connection
        Reconfigure,    // Check and update configuration
        Fallback,       // Use fallback data source
        Escalate,       // Escalate to administrator
        Abort           // Abort current transaction/operation
    };

    /// Structured FDW error information
    struct FdwError {
        FdwErrorCategory category;
        FdwErrorSeverity severity;
        std::string error_code;     // Standard error code (e.g., "HY000", "08001")
        std::string error_message;  // Human-readable error message
        std::string detail_message; // Additional technical details
        std::string hint;           // Suggestion for resolving the error
        std::string server_name;    // Associated foreign server
        std::string table_name;     // Associated foreign table (if applicable)
        std::string query;          // Query that caused the error (if applicable)
        FdwRecoveryAction recommended_action;
        std::int64_t timestamp;                                    // When the error occurred
        std::unordered_map<std::string, std::string> context_data; // Additional context
    };

    /// Connection health status
    enum class FdwConnectionStatus {
        Unknown,     // Status unknown
        Healthy,     // Connection is working normally
        Degraded,    // Connection working but with issues
        Unstable,    // Connection intermittently failing
        Failed,      // Connection has failed
        Reconnecting // Currently attempting to reconnect
    };

    /// Connection health metrics
    struct FdwConnectionHealth {
        FdwConnectionStatus status;
        std::string server_name;
        std::int64_t last_successful_operation; // Timestamp of last successful operation
        std::int64_t last_failure_time;         // Timestamp of last failure
        std::int32_t consecutive_failures;      // Number of consecutive failures
        std::int32_t total_failures;            // Total failures since connection established
        std::int32_t total_operations;          // Total operations attempted
        double success_rate;                    // Overall success rate (0.0 - 1.0)
        double average_response_time_ms;        // Average response time in milliseconds
        std::int64_t connection_uptime;         // How long connection has been active
        std::string last_error_message;         // Last error that occurred
        std::vector<FdwError> recent_errors;    // Recent error history
    };

    /// Network timeout configuration
    struct FdwTimeoutConfig {
        std::int32_t connection_timeout_ms = 30000;   // Connection establishment timeout
        std::int32_t query_timeout_ms = 60000;        // Query execution timeout
        std::int32_t transaction_timeout_ms = 300000; // Transaction timeout
        std::int32_t network_read_timeout_ms = 10000; // Network read timeout
        std::int32_t heartbeat_interval_ms = 30000;   // Heartbeat/keepalive interval
        std::int32_t retry_delay_ms = 1000;           // Base delay between retries
        std::int32_t max_retry_delay_ms = 30000;      // Maximum retry delay
        std::int32_t max_retry_attempts = 3;          // Maximum retry attempts
    };

    /// Connection recovery configuration
    struct FdwRecoveryConfig {
        bool auto_reconnect = true;                      // Automatically reconnect on failure
        std::int32_t reconnect_attempts = 3;             // Number of reconnection attempts
        std::int32_t reconnect_delay_ms = 5000;          // Delay between reconnection attempts
        std::int32_t retry_delay_ms = 1000;              // Base delay between retries
        bool exponential_backoff = true;                 // Use exponential backoff for retries
        double backoff_multiplier = 2.0;                 // Backoff multiplier
        bool circuit_breaker_enabled = true;             // Enable circuit breaker pattern
        std::int32_t circuit_breaker_threshold = 5;      // Failures before opening circuit
        std::int32_t circuit_breaker_timeout_ms = 60000; // Circuit breaker timeout
    };

    /// FDW error handler for connection recovery and error management
    class FdwErrorHandler
    {
      public:
        FdwErrorHandler();
        ~FdwErrorHandler();

        // Error reporting and logging
        void report_error(const FdwError& error);
        void report_connection_error(const std::string& server_name,
                                     const std::string& error_message,
                                     FdwErrorSeverity severity = FdwErrorSeverity::Error);
        void report_query_error(const std::string& server_name, const std::string& query,
                                const std::string& error_message,
                                FdwErrorSeverity severity = FdwErrorSeverity::Error);

        // Error retrieval and analysis
        std::vector<FdwError> get_recent_errors(const std::string& server_name = "",
                                                std::int32_t max_count = 100) const;
        std::vector<FdwError> get_errors_by_category(FdwErrorCategory category,
                                                     const std::string& server_name = "") const;
        FdwError get_last_error(const std::string& server_name = "") const;

        // Error pattern analysis
        bool is_transient_error(const FdwError& error) const;
        bool should_retry(const FdwError& error) const;
        std::int32_t get_recommended_retry_delay_ms(const FdwError& error,
                                                    std::int32_t attempt_count) const;

        // Error statistics
        double get_error_rate(const std::string& server_name,
                              std::int64_t time_window_seconds = 3600) const;
        std::unordered_map<FdwErrorCategory, std::int32_t>
        get_error_counts_by_category(const std::string& server_name = "") const;

        // Configuration
        void set_timeout_config(const FdwTimeoutConfig& config);
        void set_recovery_config(const FdwRecoveryConfig& config);
        FdwTimeoutConfig get_timeout_config() const;
        FdwRecoveryConfig get_recovery_config() const;

      private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;
    };

    /// FDW connection monitor for health tracking and diagnostics
    class FdwConnectionMonitor
    {
      public:
        FdwConnectionMonitor();
        ~FdwConnectionMonitor();

        // Connection registration and deregistration
        void register_connection(const std::string& server_name,
                                 const std::string& connection_info = "");
        void unregister_connection(const std::string& server_name);

        // Health status updates
        void update_connection_status(const std::string& server_name, FdwConnectionStatus status);
        void record_successful_operation(const std::string& server_name,
                                         double response_time_ms = 0.0);
        void record_failed_operation(const std::string& server_name,
                                     const std::string& error_message = "");

        // Health queries
        FdwConnectionHealth get_connection_health(const std::string& server_name) const;
        std::vector<FdwConnectionHealth> get_all_connection_health() const;
        std::vector<std::string> get_unhealthy_connections() const;

        // Health monitoring
        bool is_connection_healthy(const std::string& server_name) const;
        bool should_reconnect(const std::string& server_name) const;
        double get_connection_uptime_hours(const std::string& server_name) const;

        // Circuit breaker functionality
        bool is_circuit_open(const std::string& server_name) const;
        void open_circuit(const std::string& server_name);
        void close_circuit(const std::string& server_name);
        bool should_allow_request(const std::string& server_name) const;

        // Health testing
        bool test_connection_health(const std::string& server_name, std::string& error_msg);
        void schedule_health_check(const std::string& server_name);

      private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;
    };

    /// FDW diagnostic tools for troubleshooting and performance analysis
    class FdwDiagnostics
    {
      public:
        FdwDiagnostics();
        ~FdwDiagnostics();

        // Connection diagnostics
        struct ConnectionDiagnostic {
            std::string server_name;
            std::string connection_string;
            bool can_establish_connection;
            double connection_time_ms;
            bool can_execute_simple_query;
            double simple_query_time_ms;
            std::string network_latency_info;
            std::string ssl_info;
            std::string server_version;
            std::vector<std::string> supported_features;
            std::vector<std::string> warnings;
            std::vector<std::string> errors;
        };

        // Performance diagnostics
        struct PerformanceDiagnostic {
            std::string server_name;
            std::int64_t total_queries;
            double average_query_time_ms;
            double min_query_time_ms;
            double max_query_time_ms;
            std::int64_t total_rows_transferred;
            double average_throughput_rows_per_sec;
            std::int64_t total_bytes_transferred;
            double average_bandwidth_mbps;
            std::vector<std::string> slow_queries; // Queries taking longer than threshold
        };

        // Query analysis
        struct QueryAnalysis {
            std::string query;
            std::string server_name;
            double execution_time_ms;
            std::int64_t rows_returned;
            std::int64_t bytes_returned;
            bool used_pushdown;
            std::vector<std::string> pushdown_operations;
            std::vector<std::string> optimization_suggestions;
        };

        // Diagnostic operations
        ConnectionDiagnostic diagnose_connection(const std::string& server_name,
                                                 std::string& error_msg);
        PerformanceDiagnostic analyze_performance(const std::string& server_name,
                                                  std::int64_t time_window_seconds = 3600);
        QueryAnalysis analyze_query(const std::string& server_name, const std::string& query);

        // Health check operations
        bool run_connection_health_check(const std::string& server_name, std::string& result_msg);
        std::vector<std::string> run_comprehensive_diagnostics(const std::string& server_name = "");

        // Performance monitoring
        void start_performance_monitoring(const std::string& server_name);
        void stop_performance_monitoring(const std::string& server_name);
        bool is_monitoring_enabled(const std::string& server_name) const;

        // Diagnostic utilities
        std::string generate_diagnostic_report(const std::string& server_name = "");
        void export_diagnostic_data(const std::string& file_path,
                                    const std::string& server_name = "");

      private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;
    };

    /// Integrated FDW diagnostics manager
    class FdwDiagnosticsManager
    {
      public:
        FdwDiagnosticsManager();
        ~FdwDiagnosticsManager();

        // Component access
        FdwErrorHandler& get_error_handler();
        FdwConnectionMonitor& get_connection_monitor();
        FdwDiagnostics& get_diagnostics();

        // Integrated operations
        bool handle_connection_error(const std::string& server_name,
                                     const std::string& error_message,
                                     std::string& recovery_action);
        bool attempt_recovery(const std::string& server_name, std::string& result_msg);

        // Health dashboard
        struct HealthDashboard {
            std::int32_t total_servers;
            std::int32_t healthy_servers;
            std::int32_t degraded_servers;
            std::int32_t failed_servers;
            double overall_success_rate;
            std::int32_t total_errors_last_hour;
            std::vector<std::string> critical_alerts;
            std::vector<FdwConnectionHealth> connection_summary;
        };

        HealthDashboard get_health_dashboard() const;
        std::string generate_health_report() const;

      private:
        std::unique_ptr<FdwErrorHandler> error_handler_;
        std::unique_ptr<FdwConnectionMonitor> connection_monitor_;
        std::unique_ptr<FdwDiagnostics> diagnostics_;
    };

} // namespace scratchbird::engine

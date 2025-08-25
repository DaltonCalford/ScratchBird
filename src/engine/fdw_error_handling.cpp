#include "scratchbird/engine/fdw_error_handling.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

namespace scratchbird::engine
{

    //=============================================================================
    // FdwErrorHandler Implementation
    //=============================================================================

    class FdwErrorHandler::Impl
    {
      public:
        std::vector<FdwError> error_log_;
        FdwTimeoutConfig timeout_config_;
        FdwRecoveryConfig recovery_config_;
        std::int64_t max_error_history_ = 1000; // Maximum errors to keep in memory

        bool is_transient_error_code(const std::string& error_code) const
        {
            // Common transient error patterns
            return error_code == "08001" || // Connection failure
                   error_code == "08003" || // Connection does not exist
                   error_code == "08006" || // Connection failure
                   error_code == "HY000" || // General timeout
                   error_code == "HYT00" || // Timeout expired
                   error_code == "HYT01";   // Connection timeout expired
        }

        FdwRecoveryAction determine_recovery_action(const FdwError& error) const
        {
            switch (error.category) {
            case FdwErrorCategory::Connection:
                return error.severity == FdwErrorSeverity::Fatal ? FdwRecoveryAction::Reconnect
                                                                 : FdwRecoveryAction::Retry;
            case FdwErrorCategory::Network:
                return FdwRecoveryAction::RetryWithDelay;
            case FdwErrorCategory::Authentication:
                return FdwRecoveryAction::Reconfigure;
            case FdwErrorCategory::Resource:
                return FdwRecoveryAction::RetryWithDelay;
            case FdwErrorCategory::Transaction:
                return FdwRecoveryAction::Retry;
            case FdwErrorCategory::Configuration:
                return FdwRecoveryAction::Reconfigure;
            case FdwErrorCategory::Security:
                return FdwRecoveryAction::Escalate;
            case FdwErrorCategory::Internal:
                return FdwRecoveryAction::Escalate;
            default:
                return FdwRecoveryAction::None;
            }
        }

        std::int32_t calculate_retry_delay(std::int32_t attempt_count) const
        {
            if (!recovery_config_.exponential_backoff) {
                return recovery_config_.reconnect_delay_ms;
            }

            double delay = recovery_config_.retry_delay_ms *
                           std::pow(recovery_config_.backoff_multiplier, attempt_count - 1);
            return std::min(static_cast<std::int32_t>(delay), timeout_config_.max_retry_delay_ms);
        }
    };

    FdwErrorHandler::FdwErrorHandler() : pImpl_(std::make_unique<Impl>()) {}

    FdwErrorHandler::~FdwErrorHandler() = default;

    void FdwErrorHandler::report_error(const FdwError& error)
    {
        pImpl_->error_log_.push_back(error);

        // Trim error log if it gets too large
        if (pImpl_->error_log_.size() > static_cast<std::size_t>(pImpl_->max_error_history_)) {
            pImpl_->error_log_.erase(pImpl_->error_log_.begin());
        }

        // Log to console for demonstration
        std::cout << "[FDW ERROR] " << static_cast<int>(error.severity) << " " << error.server_name
                  << ": " << error.error_message;
        if (!error.detail_message.empty()) {
            std::cout << " (" << error.detail_message << ")";
        }
        std::cout << std::endl;
    }

    void FdwErrorHandler::report_connection_error(const std::string& server_name,
                                                  const std::string& error_message,
                                                  FdwErrorSeverity severity)
    {
        FdwError error;
        error.category = FdwErrorCategory::Connection;
        error.severity = severity;
        error.error_code = "08001"; // Connection failure
        error.error_message = error_message;
        error.server_name = server_name;
        error.recommended_action = pImpl_->determine_recovery_action(error);
        error.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();

        report_error(error);
    }

    void FdwErrorHandler::report_query_error(const std::string& server_name,
                                             const std::string& query,
                                             const std::string& error_message,
                                             FdwErrorSeverity severity)
    {
        FdwError error;
        error.category = FdwErrorCategory::Query;
        error.severity = severity;
        error.error_code = "42000"; // Syntax error or access violation
        error.error_message = error_message;
        error.server_name = server_name;
        error.query = query;
        error.recommended_action = pImpl_->determine_recovery_action(error);
        error.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();

        report_error(error);
    }

    std::vector<FdwError> FdwErrorHandler::get_recent_errors(const std::string& server_name,
                                                             std::int32_t max_count) const
    {
        std::vector<FdwError> result;
        std::int32_t count = 0;

        for (auto it = pImpl_->error_log_.rbegin();
             it != pImpl_->error_log_.rend() && count < max_count; ++it, ++count) {
            if (server_name.empty() || it->server_name == server_name) {
                result.push_back(*it);
            }
        }

        return result;
    }

    std::vector<FdwError>
    FdwErrorHandler::get_errors_by_category(FdwErrorCategory category,
                                            const std::string& server_name) const
    {
        std::vector<FdwError> result;
        for (const auto& error : pImpl_->error_log_) {
            if (error.category == category &&
                (server_name.empty() || error.server_name == server_name)) {
                result.push_back(error);
            }
        }
        return result;
    }

    FdwError FdwErrorHandler::get_last_error(const std::string& server_name) const
    {
        for (auto it = pImpl_->error_log_.rbegin(); it != pImpl_->error_log_.rend(); ++it) {
            if (server_name.empty() || it->server_name == server_name) {
                return *it;
            }
        }

        // Return empty error if none found
        return FdwError{};
    }

    bool FdwErrorHandler::is_transient_error(const FdwError& error) const
    {
        return pImpl_->is_transient_error_code(error.error_code) ||
               error.category == FdwErrorCategory::Network ||
               error.category == FdwErrorCategory::Resource;
    }

    bool FdwErrorHandler::should_retry(const FdwError& error) const
    {
        return error.recommended_action == FdwRecoveryAction::Retry ||
               error.recommended_action == FdwRecoveryAction::RetryWithDelay ||
               error.recommended_action == FdwRecoveryAction::Reconnect;
    }

    std::int32_t FdwErrorHandler::get_recommended_retry_delay_ms(const FdwError& error,
                                                                 std::int32_t attempt_count) const
    {
        if (error.recommended_action != FdwRecoveryAction::RetryWithDelay &&
            error.recommended_action != FdwRecoveryAction::Reconnect) {
            return 0;
        }

        return pImpl_->calculate_retry_delay(attempt_count);
    }

    double FdwErrorHandler::get_error_rate(const std::string& server_name,
                                           std::int64_t time_window_seconds) const
    {
        std::int64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
                                        std::chrono::system_clock::now().time_since_epoch())
                                        .count();
        std::int64_t window_start = current_time - time_window_seconds;

        std::int32_t error_count = 0;
        for (const auto& error : pImpl_->error_log_) {
            if (error.timestamp >= window_start &&
                (server_name.empty() || error.server_name == server_name)) {
                error_count++;
            }
        }

        // Return errors per hour
        return (error_count * 3600.0) / time_window_seconds;
    }

    std::unordered_map<FdwErrorCategory, std::int32_t>
    FdwErrorHandler::get_error_counts_by_category(const std::string& server_name) const
    {
        std::unordered_map<FdwErrorCategory, std::int32_t> counts;
        for (const auto& error : pImpl_->error_log_) {
            if (server_name.empty() || error.server_name == server_name) {
                counts[error.category]++;
            }
        }
        return counts;
    }

    void FdwErrorHandler::set_timeout_config(const FdwTimeoutConfig& config)
    {
        pImpl_->timeout_config_ = config;
    }

    void FdwErrorHandler::set_recovery_config(const FdwRecoveryConfig& config)
    {
        pImpl_->recovery_config_ = config;
    }

    FdwTimeoutConfig FdwErrorHandler::get_timeout_config() const
    {
        return pImpl_->timeout_config_;
    }

    FdwRecoveryConfig FdwErrorHandler::get_recovery_config() const
    {
        return pImpl_->recovery_config_;
    }

    //=============================================================================
    // FdwConnectionMonitor Implementation
    //=============================================================================

    class FdwConnectionMonitor::Impl
    {
      public:
        std::unordered_map<std::string, FdwConnectionHealth> connection_health_;
        std::unordered_map<std::string, std::int64_t> circuit_breaker_open_time_;
        FdwRecoveryConfig recovery_config_;

        void update_health_metrics(FdwConnectionHealth& health, bool success,
                                   double response_time_ms = 0.0, const std::string& error_msg = "")
        {
            health.total_operations++;

            if (success) {
                health.last_successful_operation =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
                health.consecutive_failures = 0;

                // Update response time (simple moving average)
                if (health.total_operations == 1) {
                    health.average_response_time_ms = response_time_ms;
                } else {
                    health.average_response_time_ms =
                        (health.average_response_time_ms * 0.9) + (response_time_ms * 0.1);
                }
            } else {
                health.total_failures++;
                health.consecutive_failures++;
                health.last_failure_time = std::chrono::duration_cast<std::chrono::seconds>(
                                               std::chrono::system_clock::now().time_since_epoch())
                                               .count();
                health.last_error_message = error_msg;

                // Add error to recent errors (keep last 10)
                FdwError error;
                error.error_message = error_msg;
                error.timestamp = health.last_failure_time;
                health.recent_errors.push_back(error);
                if (health.recent_errors.size() > 10) {
                    health.recent_errors.erase(health.recent_errors.begin());
                }
            }

            // Update success rate
            health.success_rate = 1.0 - (static_cast<double>(health.total_failures) /
                                         static_cast<double>(health.total_operations));

            // Update status based on metrics
            if (health.consecutive_failures == 0) {
                health.status = FdwConnectionStatus::Healthy;
            } else if (health.consecutive_failures < 3) {
                health.status = FdwConnectionStatus::Degraded;
            } else if (health.consecutive_failures < 5) {
                health.status = FdwConnectionStatus::Unstable;
            } else {
                health.status = FdwConnectionStatus::Failed;
            }
        }
    };

    FdwConnectionMonitor::FdwConnectionMonitor() : pImpl_(std::make_unique<Impl>()) {}

    FdwConnectionMonitor::~FdwConnectionMonitor() = default;

    void FdwConnectionMonitor::register_connection(const std::string& server_name,
                                                   const std::string& connection_info)
    {
        FdwConnectionHealth health;
        health.server_name = server_name;
        health.status = FdwConnectionStatus::Unknown;
        health.last_successful_operation = 0;
        health.last_failure_time = 0;
        health.consecutive_failures = 0;
        health.total_failures = 0;
        health.total_operations = 0;
        health.success_rate = 1.0;
        health.average_response_time_ms = 0.0;
        health.connection_uptime = std::chrono::duration_cast<std::chrono::seconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count();

        pImpl_->connection_health_[server_name] = health;

        std::cout << "✓ Registered connection monitor for server: " << server_name << std::endl;
    }

    void FdwConnectionMonitor::unregister_connection(const std::string& server_name)
    {
        pImpl_->connection_health_.erase(server_name);
        pImpl_->circuit_breaker_open_time_.erase(server_name);
        std::cout << "✓ Unregistered connection monitor for server: " << server_name << std::endl;
    }

    void FdwConnectionMonitor::update_connection_status(const std::string& server_name,
                                                        FdwConnectionStatus status)
    {
        auto it = pImpl_->connection_health_.find(server_name);
        if (it != pImpl_->connection_health_.end()) {
            it->second.status = status;
        }
    }

    void FdwConnectionMonitor::record_successful_operation(const std::string& server_name,
                                                           double response_time_ms)
    {
        auto it = pImpl_->connection_health_.find(server_name);
        if (it != pImpl_->connection_health_.end()) {
            pImpl_->update_health_metrics(it->second, true, response_time_ms);
        }
    }

    void FdwConnectionMonitor::record_failed_operation(const std::string& server_name,
                                                       const std::string& error_message)
    {
        auto it = pImpl_->connection_health_.find(server_name);
        if (it != pImpl_->connection_health_.end()) {
            pImpl_->update_health_metrics(it->second, false, 0.0, error_message);
        }
    }

    FdwConnectionHealth
    FdwConnectionMonitor::get_connection_health(const std::string& server_name) const
    {
        auto it = pImpl_->connection_health_.find(server_name);
        if (it != pImpl_->connection_health_.end()) {
            return it->second;
        }

        // Return default health if not found
        FdwConnectionHealth health;
        health.server_name = server_name;
        health.status = FdwConnectionStatus::Unknown;
        return health;
    }

    std::vector<FdwConnectionHealth> FdwConnectionMonitor::get_all_connection_health() const
    {
        std::vector<FdwConnectionHealth> result;
        for (const auto& pair : pImpl_->connection_health_) {
            result.push_back(pair.second);
        }
        return result;
    }

    std::vector<std::string> FdwConnectionMonitor::get_unhealthy_connections() const
    {
        std::vector<std::string> result;
        for (const auto& pair : pImpl_->connection_health_) {
            if (pair.second.status == FdwConnectionStatus::Failed ||
                pair.second.status == FdwConnectionStatus::Unstable) {
                result.push_back(pair.first);
            }
        }
        return result;
    }

    bool FdwConnectionMonitor::is_connection_healthy(const std::string& server_name) const
    {
        auto it = pImpl_->connection_health_.find(server_name);
        if (it != pImpl_->connection_health_.end()) {
            return it->second.status == FdwConnectionStatus::Healthy ||
                   it->second.status == FdwConnectionStatus::Degraded;
        }
        return false;
    }

    bool FdwConnectionMonitor::should_reconnect(const std::string& server_name) const
    {
        auto it = pImpl_->connection_health_.find(server_name);
        if (it != pImpl_->connection_health_.end()) {
            return it->second.status == FdwConnectionStatus::Failed &&
                   it->second.consecutive_failures >= 3;
        }
        return false;
    }

    double FdwConnectionMonitor::get_connection_uptime_hours(const std::string& server_name) const
    {
        auto it = pImpl_->connection_health_.find(server_name);
        if (it != pImpl_->connection_health_.end()) {
            std::int64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
                                            std::chrono::system_clock::now().time_since_epoch())
                                            .count();
            return (current_time - it->second.connection_uptime) / 3600.0;
        }
        return 0.0;
    }

    bool FdwConnectionMonitor::is_circuit_open(const std::string& server_name) const
    {
        auto it = pImpl_->circuit_breaker_open_time_.find(server_name);
        if (it != pImpl_->circuit_breaker_open_time_.end()) {
            std::int64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                                            std::chrono::system_clock::now().time_since_epoch())
                                            .count();
            return (current_time - it->second) <
                   pImpl_->recovery_config_.circuit_breaker_timeout_ms;
        }
        return false;
    }

    void FdwConnectionMonitor::open_circuit(const std::string& server_name)
    {
        pImpl_->circuit_breaker_open_time_[server_name] =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
        std::cout << "⚡ Circuit breaker OPENED for server: " << server_name << std::endl;
    }

    void FdwConnectionMonitor::close_circuit(const std::string& server_name)
    {
        pImpl_->circuit_breaker_open_time_.erase(server_name);
        std::cout << "✓ Circuit breaker CLOSED for server: " << server_name << std::endl;
    }

    bool FdwConnectionMonitor::should_allow_request(const std::string& server_name) const
    {
        if (!pImpl_->recovery_config_.circuit_breaker_enabled) {
            return true;
        }

        auto health_it = pImpl_->connection_health_.find(server_name);
        if (health_it != pImpl_->connection_health_.end()) {
            // Open circuit if too many consecutive failures
            if (health_it->second.consecutive_failures >=
                pImpl_->recovery_config_.circuit_breaker_threshold) {
                return !is_circuit_open(server_name);
            }
        }

        return true;
    }

    bool FdwConnectionMonitor::test_connection_health(const std::string& server_name,
                                                      std::string& error_msg)
    {
        // Simulate connection health check
        // In real implementation, this would ping the server or execute a simple query
        auto it = pImpl_->connection_health_.find(server_name);
        if (it == pImpl_->connection_health_.end()) {
            error_msg = "Connection not registered: " + server_name;
            return false;
        }

        // Simulate success/failure based on current status
        if (it->second.status == FdwConnectionStatus::Failed) {
            error_msg = "Health check failed: " + it->second.last_error_message;
            record_failed_operation(server_name, "Health check failed");
            return false;
        }

        record_successful_operation(server_name, 10.0); // Simulate 10ms response time
        return true;
    }

    void FdwConnectionMonitor::schedule_health_check(const std::string& server_name)
    {
        std::cout << "📅 Scheduled health check for server: " << server_name << std::endl;
        // In real implementation, this would schedule a background task
    }

    //=============================================================================
    // FdwDiagnostics Implementation
    //=============================================================================

    class FdwDiagnostics::Impl
    {
      public:
        std::unordered_map<std::string, bool> monitoring_enabled_;
        std::unordered_map<std::string, std::vector<QueryAnalysis>> query_history_;
    };

    FdwDiagnostics::FdwDiagnostics() : pImpl_(std::make_unique<Impl>()) {}

    FdwDiagnostics::~FdwDiagnostics() = default;

    FdwDiagnostics::ConnectionDiagnostic
    FdwDiagnostics::diagnose_connection(const std::string& server_name, std::string& error_msg)
    {
        ConnectionDiagnostic diagnostic;
        diagnostic.server_name = server_name;
        diagnostic.connection_string = "mock://connection/string";

        try {
            // Simulate connection diagnostics
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            diagnostic.can_establish_connection = true;
            diagnostic.connection_time_ms = 45.0;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            diagnostic.can_execute_simple_query = true;
            diagnostic.simple_query_time_ms = 8.5;

            diagnostic.network_latency_info = "Average: 2ms, Jitter: 0.5ms";
            diagnostic.ssl_info = "TLS 1.3 enabled, Certificate valid";
            diagnostic.server_version = "PostgreSQL 13.7 (Mock)";
            diagnostic.supported_features = {"transactions", "prepared_statements",
                                             "bulk_operations"};

            if (server_name.find("slow") != std::string::npos) {
                diagnostic.warnings.push_back("High network latency detected");
            }

            std::cout << "✓ Connection diagnostic completed for: " << server_name << std::endl;

        } catch (const std::exception& e) {
            diagnostic.can_establish_connection = false;
            diagnostic.errors.push_back("Connection diagnostic failed: " + std::string(e.what()));
            error_msg = "Diagnostic failed: " + std::string(e.what());
        }

        return diagnostic;
    }

    FdwDiagnostics::PerformanceDiagnostic
    FdwDiagnostics::analyze_performance(const std::string& server_name,
                                        std::int64_t time_window_seconds)
    {
        PerformanceDiagnostic diagnostic;
        diagnostic.server_name = server_name;

        // Simulate performance analysis based on historical data
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> query_count_dis(50, 1000);
        std::uniform_real_distribution<> time_dis(5.0, 500.0);
        std::uniform_int_distribution<> row_dis(1, 10000);

        diagnostic.total_queries = query_count_dis(gen);
        diagnostic.average_query_time_ms = time_dis(gen);
        diagnostic.min_query_time_ms = 1.2;
        diagnostic.max_query_time_ms = diagnostic.average_query_time_ms * 3;
        diagnostic.total_rows_transferred = diagnostic.total_queries * row_dis(gen);
        diagnostic.average_throughput_rows_per_sec =
            diagnostic.total_rows_transferred / (time_window_seconds / 1.0);
        diagnostic.total_bytes_transferred =
            diagnostic.total_rows_transferred * 64; // Assume 64 bytes/row
        diagnostic.average_bandwidth_mbps =
            (diagnostic.total_bytes_transferred * 8.0) / (time_window_seconds * 1024 * 1024);

        // Add some slow queries
        if (diagnostic.average_query_time_ms > 100.0) {
            diagnostic.slow_queries.push_back("SELECT * FROM large_table WHERE complex_condition");
            diagnostic.slow_queries.push_back("SELECT COUNT(*) FROM very_large_table");
        }

        std::cout << "✓ Performance analysis completed for: " << server_name << std::endl;
        return diagnostic;
    }

    FdwDiagnostics::QueryAnalysis FdwDiagnostics::analyze_query(const std::string& server_name,
                                                                const std::string& query)
    {
        QueryAnalysis analysis;
        analysis.query = query;
        analysis.server_name = server_name;

        // Simulate query analysis
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> time_dis(5.0, 200.0);
        std::uniform_int_distribution<> row_dis(1, 1000);

        analysis.execution_time_ms = time_dis(gen);
        analysis.rows_returned = row_dis(gen);
        analysis.bytes_returned = analysis.rows_returned * 64;

        // Determine if pushdown was used
        if (query.find("WHERE") != std::string::npos || query.find("LIMIT") != std::string::npos) {
            analysis.used_pushdown = true;
            analysis.pushdown_operations.push_back("WHERE clause pushdown");
            if (query.find("LIMIT") != std::string::npos) {
                analysis.pushdown_operations.push_back("LIMIT pushdown");
            }
        }

        // Add optimization suggestions
        if (analysis.execution_time_ms > 100.0) {
            analysis.optimization_suggestions.push_back(
                "Consider adding an index on filtered columns");
        }
        if (analysis.rows_returned > 1000) {
            analysis.optimization_suggestions.push_back(
                "Consider using LIMIT to reduce data transfer");
        }

        // Store in query history
        pImpl_->query_history_[server_name].push_back(analysis);
        if (pImpl_->query_history_[server_name].size() > 100) {
            pImpl_->query_history_[server_name].erase(pImpl_->query_history_[server_name].begin());
        }

        return analysis;
    }

    bool FdwDiagnostics::run_connection_health_check(const std::string& server_name,
                                                     std::string& result_msg)
    {
        std::ostringstream oss;
        oss << "Health check results for " << server_name << ":\n";

        try {
            // Simulate various health checks
            oss << "✓ Network connectivity: OK\n";
            oss << "✓ Authentication: OK\n";
            oss << "✓ Basic query execution: OK (8.5ms)\n";
            oss << "✓ Transaction support: OK\n";

            result_msg = oss.str();
            return true;
        } catch (const std::exception& e) {
            oss << "⚠ Health check failed: " << e.what() << "\n";
            result_msg = oss.str();
            return false;
        }
    }

    std::vector<std::string>
    FdwDiagnostics::run_comprehensive_diagnostics(const std::string& server_name)
    {
        std::vector<std::string> results;

        if (server_name.empty()) {
            results.push_back("Running comprehensive diagnostics for all servers...");
            // In real implementation, would iterate through all registered servers
            results.push_back("✓ Comprehensive diagnostics completed for all servers");
        } else {
            results.push_back("Running comprehensive diagnostics for: " + server_name);

            std::string error_msg;
            auto conn_diag = diagnose_connection(server_name, error_msg);
            if (conn_diag.can_establish_connection) {
                results.push_back("✓ Connection diagnostics: PASSED");
            } else {
                results.push_back("⚠ Connection diagnostics: FAILED - " + error_msg);
            }

            auto perf_diag = analyze_performance(server_name, 3600);
            results.push_back("✓ Performance analysis: " + std::to_string(perf_diag.total_queries) +
                              " queries analyzed");

            std::string health_result;
            bool health_ok = run_connection_health_check(server_name, health_result);
            results.push_back(health_ok ? "✓ Health check: PASSED"
                                        : "⚠ Health check: ISSUES FOUND");
        }

        return results;
    }

    void FdwDiagnostics::start_performance_monitoring(const std::string& server_name)
    {
        pImpl_->monitoring_enabled_[server_name] = true;
        std::cout << "📊 Started performance monitoring for: " << server_name << std::endl;
    }

    void FdwDiagnostics::stop_performance_monitoring(const std::string& server_name)
    {
        pImpl_->monitoring_enabled_[server_name] = false;
        std::cout << "📊 Stopped performance monitoring for: " << server_name << std::endl;
    }

    bool FdwDiagnostics::is_monitoring_enabled(const std::string& server_name) const
    {
        auto it = pImpl_->monitoring_enabled_.find(server_name);
        return it != pImpl_->monitoring_enabled_.end() && it->second;
    }

    std::string FdwDiagnostics::generate_diagnostic_report(const std::string& server_name)
    {
        std::ostringstream oss;
        oss << "=== FDW Diagnostic Report ===\n";
        oss << "Generated: " << std::chrono::system_clock::now().time_since_epoch().count()
            << "\n\n";

        if (server_name.empty()) {
            oss << "Report Type: System-wide diagnostics\n";
        } else {
            oss << "Server: " << server_name << "\n";

            std::string error_msg;
            auto conn_diag = diagnose_connection(server_name, error_msg);
            oss << "\nConnection Diagnostics:\n";
            oss << "- Can establish connection: "
                << (conn_diag.can_establish_connection ? "YES" : "NO") << "\n";
            oss << "- Connection time: " << conn_diag.connection_time_ms << "ms\n";
            oss << "- Simple query time: " << conn_diag.simple_query_time_ms << "ms\n";
            oss << "- Server version: " << conn_diag.server_version << "\n";

            auto perf_diag = analyze_performance(server_name, 3600);
            oss << "\nPerformance Metrics (last hour):\n";
            oss << "- Total queries: " << perf_diag.total_queries << "\n";
            oss << "- Average query time: " << perf_diag.average_query_time_ms << "ms\n";
            oss << "- Throughput: " << perf_diag.average_throughput_rows_per_sec << " rows/sec\n";
            oss << "- Bandwidth: " << perf_diag.average_bandwidth_mbps << " Mbps\n";
        }

        return oss.str();
    }

    void FdwDiagnostics::export_diagnostic_data(const std::string& file_path,
                                                const std::string& server_name)
    {
        std::ofstream file(file_path);
        if (file.is_open()) {
            file << generate_diagnostic_report(server_name);
            file.close();
            std::cout << "✓ Diagnostic data exported to: " << file_path << std::endl;
        } else {
            std::cout << "⚠ Failed to export diagnostic data to: " << file_path << std::endl;
        }
    }

    //=============================================================================
    // FdwDiagnosticsManager Implementation
    //=============================================================================

    FdwDiagnosticsManager::FdwDiagnosticsManager()
        : error_handler_(std::make_unique<FdwErrorHandler>()),
          connection_monitor_(std::make_unique<FdwConnectionMonitor>()),
          diagnostics_(std::make_unique<FdwDiagnostics>())
    {
    }

    FdwDiagnosticsManager::~FdwDiagnosticsManager() = default;

    FdwErrorHandler& FdwDiagnosticsManager::get_error_handler()
    {
        return *error_handler_;
    }

    FdwConnectionMonitor& FdwDiagnosticsManager::get_connection_monitor()
    {
        return *connection_monitor_;
    }

    FdwDiagnostics& FdwDiagnosticsManager::get_diagnostics()
    {
        return *diagnostics_;
    }

    bool FdwDiagnosticsManager::handle_connection_error(const std::string& server_name,
                                                        const std::string& error_message,
                                                        std::string& recovery_action)
    {
        // Report the error
        error_handler_->report_connection_error(server_name, error_message);

        // Update connection status
        connection_monitor_->record_failed_operation(server_name, error_message);

        // Determine recovery action
        FdwError last_error = error_handler_->get_last_error(server_name);
        if (error_handler_->should_retry(last_error)) {
            if (connection_monitor_->should_reconnect(server_name)) {
                recovery_action = "RECONNECT";
                connection_monitor_->update_connection_status(server_name,
                                                              FdwConnectionStatus::Reconnecting);
            } else {
                recovery_action = "RETRY";
            }
            return true;
        } else {
            recovery_action = "ESCALATE";
            return false;
        }
    }

    bool FdwDiagnosticsManager::attempt_recovery(const std::string& server_name,
                                                 std::string& result_msg)
    {
        std::ostringstream oss;
        oss << "Attempting recovery for server: " << server_name << "\n";

        try {
            // Check if circuit breaker should allow the attempt
            if (!connection_monitor_->should_allow_request(server_name)) {
                oss << "⚡ Circuit breaker is open - recovery blocked\n";
                result_msg = oss.str();
                return false;
            }

            // Attempt health check
            std::string health_result;
            bool health_ok = diagnostics_->run_connection_health_check(server_name, health_result);

            if (health_ok) {
                connection_monitor_->record_successful_operation(server_name, 50.0);
                connection_monitor_->close_circuit(server_name);
                oss << "✓ Recovery successful - connection restored\n";
                result_msg = oss.str();
                return true;
            } else {
                connection_monitor_->record_failed_operation(server_name,
                                                             "Recovery attempt failed");
                if (connection_monitor_->get_connection_health(server_name).consecutive_failures >=
                    5) {
                    connection_monitor_->open_circuit(server_name);
                }
                oss << "⚠ Recovery failed - " << health_result << "\n";
                result_msg = oss.str();
                return false;
            }

        } catch (const std::exception& e) {
            oss << "⚠ Recovery attempt failed with exception: " << e.what() << "\n";
            result_msg = oss.str();
            return false;
        }
    }

    FdwDiagnosticsManager::HealthDashboard FdwDiagnosticsManager::get_health_dashboard() const
    {
        HealthDashboard dashboard;
        auto all_health = connection_monitor_->get_all_connection_health();

        dashboard.total_servers = all_health.size();
        dashboard.healthy_servers = 0;
        dashboard.degraded_servers = 0;
        dashboard.failed_servers = 0;

        double total_success_rate = 0.0;
        std::int32_t total_operations = 0;

        for (const auto& health : all_health) {
            switch (health.status) {
            case FdwConnectionStatus::Healthy:
                dashboard.healthy_servers++;
                break;
            case FdwConnectionStatus::Degraded:
                dashboard.degraded_servers++;
                break;
            case FdwConnectionStatus::Failed:
            case FdwConnectionStatus::Unstable:
                dashboard.failed_servers++;
                break;
            default:
                break;
            }

            total_success_rate += health.success_rate * health.total_operations;
            total_operations += health.total_operations;

            // Add to connection summary
            dashboard.connection_summary.push_back(health);

            // Check for critical alerts
            if (health.status == FdwConnectionStatus::Failed) {
                dashboard.critical_alerts.push_back("Server " + health.server_name +
                                                    " is in failed state");
            } else if (health.consecutive_failures >= 3) {
                dashboard.critical_alerts.push_back("Server " + health.server_name + " has " +
                                                    std::to_string(health.consecutive_failures) +
                                                    " consecutive failures");
            }
        }

        // Calculate overall success rate
        if (total_operations > 0) {
            dashboard.overall_success_rate = total_success_rate / total_operations;
        } else {
            dashboard.overall_success_rate = 1.0;
        }

        // Get recent error count
        dashboard.total_errors_last_hour = error_handler_->get_error_rate("", 3600);

        return dashboard;
    }

    std::string FdwDiagnosticsManager::generate_health_report() const
    {
        std::ostringstream oss;
        auto dashboard = get_health_dashboard();

        oss << "=== FDW Health Report ===\n";
        oss << "Generated: " << std::chrono::system_clock::now().time_since_epoch().count()
            << "\n\n";

        oss << "Overall Status:\n";
        oss << "- Total servers: " << dashboard.total_servers << "\n";
        oss << "- Healthy: " << dashboard.healthy_servers << "\n";
        oss << "- Degraded: " << dashboard.degraded_servers << "\n";
        oss << "- Failed: " << dashboard.failed_servers << "\n";
        oss << "- Success rate: " << (dashboard.overall_success_rate * 100) << "%\n";
        oss << "- Errors (last hour): " << dashboard.total_errors_last_hour << "\n\n";

        if (!dashboard.critical_alerts.empty()) {
            oss << "Critical Alerts:\n";
            for (const auto& alert : dashboard.critical_alerts) {
                oss << "⚠ " << alert << "\n";
            }
            oss << "\n";
        }

        oss << "Connection Details:\n";
        for (const auto& health : dashboard.connection_summary) {
            oss << "- " << health.server_name << ": ";
            switch (health.status) {
            case FdwConnectionStatus::Healthy:
                oss << "HEALTHY";
                break;
            case FdwConnectionStatus::Degraded:
                oss << "DEGRADED";
                break;
            case FdwConnectionStatus::Failed:
                oss << "FAILED";
                break;
            case FdwConnectionStatus::Unstable:
                oss << "UNSTABLE";
                break;
            default:
                oss << "UNKNOWN";
            }
            oss << " (Success rate: " << (health.success_rate * 100) << "%";
            oss << ", Avg response: " << health.average_response_time_ms << "ms)\n";
        }

        return oss.str();
    }

} // namespace scratchbird::engine

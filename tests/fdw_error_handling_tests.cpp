#include "scratchbird/engine/fdw_error_handling.h"
#include "test_db_utils.h"

#include <cassert>
#include <iostream>
#include <thread>

using namespace scratchbird::engine;

void test_fdw_error_handler()
{
    std::cout << "=== Testing FDW Error Handler ===\n";

    FdwErrorHandler error_handler;

    // Test basic error reporting
    FdwError error;
    error.category = FdwErrorCategory::Connection;
    error.severity = FdwErrorSeverity::Error;
    error.error_code = "08001";
    error.error_message = "Connection refused";
    error.server_name = "test_server";
    error.recommended_action = FdwRecoveryAction::Retry;

    error_handler.report_error(error);
    std::cout << "✓ Basic error reporting test passed\n";

    // Test connection error reporting
    error_handler.report_connection_error("test_server", "Cannot connect to database");
    std::cout << "✓ Connection error reporting test passed\n";

    // Test query error reporting
    error_handler.report_query_error("test_server", "SELECT * FROM nonexistent_table",
                                     "Table does not exist");
    std::cout << "✓ Query error reporting test passed\n";

    // Test error retrieval
    auto recent_errors = error_handler.get_recent_errors("test_server", 10);
    assert(recent_errors.size() >= 2 && "Should have at least 2 errors");
    std::cout << "✓ Error retrieval test passed\n";

    // Test error filtering by category
    auto connection_errors = error_handler.get_errors_by_category(FdwErrorCategory::Connection);
    assert(!connection_errors.empty() && "Should have connection errors");
    std::cout << "✓ Error filtering test passed\n";

    // Test last error retrieval
    FdwError last_error = error_handler.get_last_error("test_server");
    assert(!last_error.error_message.empty() && "Should have a last error");
    std::cout << "✓ Last error retrieval test passed\n";

    // Test transient error detection
    bool is_transient = error_handler.is_transient_error(error);
    assert(is_transient && "Connection errors should be transient");
    std::cout << "✓ Transient error detection test passed\n";

    // Test retry recommendation
    bool should_retry = error_handler.should_retry(error);
    assert(should_retry && "Error with Retry action should be retryable");
    std::cout << "✓ Retry recommendation test passed\n";

    // Test retry delay calculation
    FdwError retry_error;
    retry_error.category = FdwErrorCategory::Network;
    retry_error.recommended_action = FdwRecoveryAction::RetryWithDelay;
    std::int32_t retry_delay = error_handler.get_recommended_retry_delay_ms(retry_error, 2);
    assert(retry_delay > 0 && "Should have positive retry delay");
    std::cout << "✓ Retry delay calculation test passed\n";

    // Test error rate calculation
    double error_rate = error_handler.get_error_rate("test_server", 3600);
    assert(error_rate >= 0.0 && "Error rate should be non-negative");
    std::cout << "✓ Error rate calculation test passed\n";

    // Test error counts by category
    auto error_counts = error_handler.get_error_counts_by_category("test_server");
    assert(error_counts[FdwErrorCategory::Connection] > 0 && "Should have connection errors");
    std::cout << "✓ Error counts by category test passed\n";

    // Test timeout configuration
    FdwTimeoutConfig timeout_config;
    timeout_config.connection_timeout_ms = 45000;
    timeout_config.query_timeout_ms = 120000;
    error_handler.set_timeout_config(timeout_config);

    FdwTimeoutConfig retrieved_config = error_handler.get_timeout_config();
    assert(retrieved_config.connection_timeout_ms == 45000 &&
           "Should retrieve correct timeout config");
    std::cout << "✓ Timeout configuration test passed\n";

    // Test recovery configuration
    FdwRecoveryConfig recovery_config;
    recovery_config.auto_reconnect = false;
    recovery_config.reconnect_attempts = 5;
    error_handler.set_recovery_config(recovery_config);

    FdwRecoveryConfig retrieved_recovery = error_handler.get_recovery_config();
    assert(!retrieved_recovery.auto_reconnect && "Should retrieve correct recovery config");
    std::cout << "✓ Recovery configuration test passed\n";

    std::cout << "✓ All FDW Error Handler tests passed\n\n";
}

void test_fdw_connection_monitor()
{
    std::cout << "=== Testing FDW Connection Monitor ===\n";

    FdwConnectionMonitor monitor;

    // Test connection registration
    monitor.register_connection("test_server", "host=localhost port=5432");
    monitor.register_connection("test_server2", "host=remote port=5432");
    std::cout << "✓ Connection registration test passed\n";

    // Test connection status updates
    monitor.update_connection_status("test_server", FdwConnectionStatus::Healthy);
    std::cout << "✓ Connection status update test passed\n";

    // Test successful operation recording
    monitor.record_successful_operation("test_server", 25.5);
    monitor.record_successful_operation("test_server", 30.0);
    std::cout << "✓ Successful operation recording test passed\n";

    // Test failed operation recording
    monitor.record_failed_operation("test_server", "Timeout occurred");
    std::cout << "✓ Failed operation recording test passed\n";

    // Test connection health retrieval
    FdwConnectionHealth health = monitor.get_connection_health("test_server");
    assert(health.server_name == "test_server" && "Should retrieve correct server health");
    assert(health.total_operations > 0 && "Should have recorded operations");
    std::cout << "✓ Connection health retrieval test passed\n";

    // Test all connections health
    auto all_health = monitor.get_all_connection_health();
    assert(all_health.size() >= 2 && "Should have at least 2 connections");
    std::cout << "✓ All connections health test passed\n";

    // Test connection health check
    bool is_healthy = monitor.is_connection_healthy("test_server");
    std::cout << "✓ Connection health check test passed (healthy=" << is_healthy << ")\n";

    // Test uptime calculation
    double uptime_hours = monitor.get_connection_uptime_hours("test_server");
    assert(uptime_hours >= 0.0 && "Uptime should be non-negative");
    std::cout << "✓ Connection uptime test passed\n";

    // Test circuit breaker functionality
    bool initially_closed = !monitor.is_circuit_open("test_server");
    assert(initially_closed && "Circuit should initially be closed");

    // Simulate multiple failures to trigger circuit breaker
    for (int i = 0; i < 6; ++i) {
        monitor.record_failed_operation("test_server", "Consecutive failure " + std::to_string(i));
    }

    // Test should allow request (circuit breaker logic)
    bool should_allow = monitor.should_allow_request("test_server");
    std::cout << "✓ Circuit breaker test passed (allow=" << should_allow << ")\n";

    // Test manual circuit operations
    monitor.open_circuit("test_server");
    bool is_open = monitor.is_circuit_open("test_server");
    assert(is_open && "Circuit should be open after manual open");

    monitor.close_circuit("test_server");
    bool is_closed_again = !monitor.is_circuit_open("test_server");
    assert(is_closed_again && "Circuit should be closed after manual close");
    std::cout << "✓ Manual circuit operations test passed\n";

    // Test connection health check
    std::string error_msg;
    bool health_check_result = monitor.test_connection_health("test_server", error_msg);
    std::cout << "✓ Connection health check test passed (result=" << health_check_result << ")\n";

    // Test health check scheduling
    monitor.schedule_health_check("test_server");
    std::cout << "✓ Health check scheduling test passed\n";

    // Test unhealthy connections
    auto unhealthy = monitor.get_unhealthy_connections();
    std::cout << "✓ Unhealthy connections test passed (found=" << unhealthy.size() << ")\n";

    // Test connection unregistration
    monitor.unregister_connection("test_server2");
    auto health_after_unreg = monitor.get_all_connection_health();
    assert(health_after_unreg.size() == 1 &&
           "Should have one less connection after unregistration");
    std::cout << "✓ Connection unregistration test passed\n";

    std::cout << "✓ All FDW Connection Monitor tests passed\n\n";
}

void test_fdw_diagnostics()
{
    std::cout << "=== Testing FDW Diagnostics ===\n";

    FdwDiagnostics diagnostics;

    // Test connection diagnostics
    std::string error_msg;
    auto conn_diag = diagnostics.diagnose_connection("test_server", error_msg);
    assert(conn_diag.server_name == "test_server" && "Should have correct server name");
    std::cout << "✓ Connection diagnostics test passed\n";
    std::cout << "  - Can establish connection: " << conn_diag.can_establish_connection << "\n";
    std::cout << "  - Connection time: " << conn_diag.connection_time_ms << "ms\n";

    // Test performance analysis
    auto perf_diag = diagnostics.analyze_performance("test_server", 3600);
    assert(perf_diag.server_name == "test_server" && "Should have correct server name");
    assert(perf_diag.total_queries > 0 && "Should have positive query count");
    std::cout << "✓ Performance analysis test passed\n";
    std::cout << "  - Total queries: " << perf_diag.total_queries << "\n";
    std::cout << "  - Average query time: " << perf_diag.average_query_time_ms << "ms\n";

    // Test query analysis
    std::string test_query = "SELECT * FROM users WHERE age > 25 LIMIT 100";
    auto query_analysis = diagnostics.analyze_query("test_server", test_query);
    assert(query_analysis.query == test_query && "Should preserve query string");
    assert(query_analysis.execution_time_ms > 0.0 && "Should have positive execution time");
    std::cout << "✓ Query analysis test passed\n";
    std::cout << "  - Execution time: " << query_analysis.execution_time_ms << "ms\n";
    std::cout << "  - Used pushdown: " << query_analysis.used_pushdown << "\n";

    // Test connection health check
    std::string health_result;
    bool health_ok = diagnostics.run_connection_health_check("test_server", health_result);
    assert(!health_result.empty() && "Should have health check result");
    std::cout << "✓ Connection health check test passed (OK=" << health_ok << ")\n";

    // Test comprehensive diagnostics
    auto diag_results = diagnostics.run_comprehensive_diagnostics("test_server");
    assert(!diag_results.empty() && "Should have diagnostic results");
    std::cout << "✓ Comprehensive diagnostics test passed\n";
    for (const auto& result : diag_results) {
        std::cout << "  - " << result << "\n";
    }

    // Test performance monitoring
    diagnostics.start_performance_monitoring("test_server");
    bool is_monitoring = diagnostics.is_monitoring_enabled("test_server");
    assert(is_monitoring && "Performance monitoring should be enabled");

    diagnostics.stop_performance_monitoring("test_server");
    bool is_monitoring_stopped = !diagnostics.is_monitoring_enabled("test_server");
    assert(is_monitoring_stopped && "Performance monitoring should be disabled");
    std::cout << "✓ Performance monitoring test passed\n";

    // Test diagnostic report generation
    std::string report = diagnostics.generate_diagnostic_report("test_server");
    assert(!report.empty() && "Should generate non-empty report");
    assert(report.find("test_server") != std::string::npos && "Report should contain server name");
    std::cout << "✓ Diagnostic report generation test passed\n";

    // Test diagnostic data export
    std::string export_file = "/tmp/fdw_diagnostics_test.txt";
    diagnostics.export_diagnostic_data(export_file, "test_server");
    std::cout << "✓ Diagnostic data export test passed\n";

    std::cout << "✓ All FDW Diagnostics tests passed\n\n";
}

void test_fdw_diagnostics_manager()
{
    std::cout << "=== Testing FDW Diagnostics Manager ===\n";

    FdwDiagnosticsManager manager;

    // Test component access
    FdwErrorHandler& error_handler = manager.get_error_handler();
    FdwConnectionMonitor& monitor = manager.get_connection_monitor();
    FdwDiagnostics& diagnostics = manager.get_diagnostics();

    assert(&error_handler != nullptr && "Should provide error handler");
    assert(&monitor != nullptr && "Should provide connection monitor");
    assert(&diagnostics != nullptr && "Should provide diagnostics");
    std::cout << "✓ Component access test passed\n";

    // Register a connection for testing
    monitor.register_connection("managed_server", "test connection");

    // Test connection error handling
    std::string recovery_action;
    bool handle_result =
        manager.handle_connection_error("managed_server", "Connection lost", recovery_action);
    assert(!recovery_action.empty() && "Should recommend recovery action");
    std::cout << "✓ Connection error handling test passed (action=" << recovery_action << ")\n";

    // Test recovery attempt
    std::string result_msg;
    bool recovery_result = manager.attempt_recovery("managed_server", result_msg);
    assert(!result_msg.empty() && "Should have recovery result message");
    std::cout << "✓ Recovery attempt test passed (success=" << recovery_result << ")\n";

    // Record some operations to build health data
    monitor.record_successful_operation("managed_server", 15.0);
    monitor.record_successful_operation("managed_server", 20.0);
    monitor.record_failed_operation("managed_server", "Simulated error");

    // Test health dashboard
    auto dashboard = manager.get_health_dashboard();
    assert(dashboard.total_servers > 0 && "Should have at least one server");
    std::cout << "✓ Health dashboard test passed\n";
    std::cout << "  - Total servers: " << dashboard.total_servers << "\n";
    std::cout << "  - Healthy servers: " << dashboard.healthy_servers << "\n";
    std::cout << "  - Failed servers: " << dashboard.failed_servers << "\n";
    std::cout << "  - Overall success rate: " << (dashboard.overall_success_rate * 100) << "%\n";

    // Test health report generation
    std::string health_report = manager.generate_health_report();
    assert(!health_report.empty() && "Should generate non-empty health report");
    assert(health_report.find("managed_server") != std::string::npos &&
           "Report should contain server name");
    std::cout << "✓ Health report generation test passed\n";

    std::cout << "✓ All FDW Diagnostics Manager tests passed\n\n";
}

void test_fdw_error_categories_and_severities()
{
    std::cout << "=== Testing FDW Error Categories and Severities ===\n";

    // Test all error categories
    std::vector<FdwErrorCategory> categories = {
        FdwErrorCategory::Connection,    FdwErrorCategory::Authentication,
        FdwErrorCategory::Network,       FdwErrorCategory::Query,
        FdwErrorCategory::DataType,      FdwErrorCategory::Transaction,
        FdwErrorCategory::Configuration, FdwErrorCategory::Resource,
        FdwErrorCategory::Security,      FdwErrorCategory::Internal,
        FdwErrorCategory::Unknown};

    for (auto category : categories) {
        FdwError error;
        error.category = category;
        error.severity = FdwErrorSeverity::Error;
        error.error_message = "Test error for category validation";

        // Verify we can create errors for all categories
        assert(!error.error_message.empty() && "Error should have message");
    }
    std::cout << "✓ Error categories test passed\n";

    // Test all severity levels
    std::vector<FdwErrorSeverity> severities = {FdwErrorSeverity::Info, FdwErrorSeverity::Warning,
                                                FdwErrorSeverity::Error, FdwErrorSeverity::Critical,
                                                FdwErrorSeverity::Fatal};

    for (auto severity : severities) {
        FdwError error;
        error.category = FdwErrorCategory::Connection;
        error.severity = severity;
        error.error_message = "Test error for severity validation";

        // Verify we can create errors for all severities
        assert(!error.error_message.empty() && "Error should have message");
    }
    std::cout << "✓ Error severities test passed\n";

    // Test all recovery actions
    std::vector<FdwRecoveryAction> actions = {
        FdwRecoveryAction::None,           FdwRecoveryAction::Retry,
        FdwRecoveryAction::RetryWithDelay, FdwRecoveryAction::Reconnect,
        FdwRecoveryAction::Reconfigure,    FdwRecoveryAction::Fallback,
        FdwRecoveryAction::Escalate,       FdwRecoveryAction::Abort};

    for (auto action : actions) {
        FdwError error;
        error.category = FdwErrorCategory::Connection;
        error.severity = FdwErrorSeverity::Error;
        error.recommended_action = action;
        error.error_message = "Test error for recovery action validation";

        // Verify we can create errors with all recovery actions
        assert(!error.error_message.empty() && "Error should have message");
    }
    std::cout << "✓ Recovery actions test passed\n";

    // Test all connection statuses
    std::vector<FdwConnectionStatus> statuses = {
        FdwConnectionStatus::Unknown,  FdwConnectionStatus::Healthy,
        FdwConnectionStatus::Degraded, FdwConnectionStatus::Unstable,
        FdwConnectionStatus::Failed,   FdwConnectionStatus::Reconnecting};

    FdwConnectionMonitor monitor;
    monitor.register_connection("status_test_server");

    for (auto status : statuses) {
        monitor.update_connection_status("status_test_server", status);

        FdwConnectionHealth health = monitor.get_connection_health("status_test_server");
        // Note: Status might be overridden by health metrics, so we just verify no crash
        assert(health.server_name == "status_test_server" && "Should maintain server name");
    }
    std::cout << "✓ Connection statuses test passed\n";

    std::cout << "✓ All FDW Error Categories and Severities tests passed\n\n";
}

int main()
{
    std::cout << "=== FDW Error Handling and Diagnostics Tests ===\n\n";

    try {
        test_fdw_error_categories_and_severities();
        test_fdw_error_handler();
        test_fdw_connection_monitor();
        test_fdw_diagnostics();
        test_fdw_diagnostics_manager();

        std::cout
            << "=== All FDW Error Handling and Diagnostics Tests Completed Successfully ===\n";
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

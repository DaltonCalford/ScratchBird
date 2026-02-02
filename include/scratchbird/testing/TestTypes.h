/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/*
 * ScratchBird Database Engine
 * Test Framework Types and Structures
 * Copyright (c) 2025 ScratchBird Project
 */

#ifndef SCRATCHBIRD_TESTING_TESTTYPES_H
#define SCRATCHBIRD_TESTING_TESTTYPES_H

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <optional>
#include <functional>
#include <variant>
#include <memory>

namespace scratchbird {
namespace testing {

//=============================================================================
// Test Result Enums
//=============================================================================

enum class TestStatus {
    PENDING,        // Not yet run
    RUNNING,        // Currently executing
    PASSED,         // Test passed
    FAILED,         // Test failed (assertion)
    ERROR,          // Test error (exception/crash)
    SKIPPED,        // Test skipped (precondition)
    TIMEOUT,        // Test timed out
    BLOCKED         // Test blocked by dependency
};

enum class TestPriority {
    P0_CRITICAL,    // Must pass for release
    P1_HIGH,        // Should pass for release
    P2_MEDIUM,      // Nice to have
    P3_LOW          // Optional/stretch
};

enum class TestCategory {
    UNIT,           // Unit tests
    INTEGRATION,    // Integration tests
    PROTOCOL,       // Protocol compliance
    AUTHENTICATION, // Auth tests
    AUTHORIZATION,  // Permission tests
    LOAD,           // Load tests
    STRESS,         // Stress tests
    ENDURANCE,      // Long-running tests
    SECURITY,       // Security/penetration tests
    PERFORMANCE,    // Benchmark tests
    REGRESSION      // Regression tests
};

enum class Protocol {
    POSTGRESQL,     // Port 5432
    MYSQL,          // Port 3306
    TDS,            // Port 1433 (SQL Server)
    FIREBIRD,       // Port 3050
    NATIVE          // Port 3092 (ScratchBird)
};

enum class AuthMethod {
    PASSWORD_PLAIN,
    PASSWORD_MD5,
    SCRAM_SHA_256,
    SCRAM_SHA_512,
    MYSQL_NATIVE,
    CACHING_SHA2,
    NTLM,
    KERBEROS,
    LDAP,
    CERTIFICATE,
    SAML,
    OAUTH2,
    MFA_TOTP
};

enum class SecurityTestType {
    NETWORK,            // Network security tests
    AUTHENTICATION,     // Auth attack tests
    SQL_INJECTION,      // SQLi tests
    PROTOCOL_FUZZ,      // Protocol fuzzing
    AUTHORIZATION,      // Authz bypass tests
    DENIAL_OF_SERVICE,  // DoS tests
    DATA_PROTECTION     // Data leak tests
};

enum class LoadTestType {
    CONNECTION,         // Connection load
    QUERY,              // Query throughput
    MIXED,              // Mixed workload
    BULK,               // Bulk operations
    PROTOCOL_SPECIFIC   // Per-protocol tests
};

enum class StressTestType {
    CPU,                // CPU stress
    MEMORY,             // Memory pressure
    DISK_IO,            // I/O saturation
    NETWORK,            // Network congestion
    MIXED,              // Combined stress
    CHECKPOINT,         // Under checkpoint
    GARBAGE_COLLECTION  // GC stress
};

//=============================================================================
// Test Configuration
//=============================================================================

struct TestConfig {
    // Execution settings
    int timeout_ms = 30000;                     // Default 30 second timeout
    int retry_count = 0;                        // Retries on failure
    bool parallel_safe = true;                  // Can run in parallel
    bool requires_network = false;              // Needs network
    bool requires_database = true;              // Needs database

    // Database configuration
    std::string db_config = "tiny";             // tiny, small, medium, large
    int buffer_pool_mb = 1;                     // Buffer pool size
    int max_connections = 10;                   // Max connections

    // Test data
    std::string dataset = "tiny";               // Test dataset

    // Dependencies
    std::vector<std::string> depends_on;        // Test dependencies
    std::vector<std::string> tags;              // Test tags
};

struct LoadTestConfig {
    // Connection parameters
    int concurrent_connections = 10;
    int total_connections = 100;
    int connection_rate = 10;                   // Connections per second

    // Query parameters
    int queries_per_connection = 100;
    int query_rate = 1000;                      // Queries per second target

    // Duration
    int duration_seconds = 60;
    int warmup_seconds = 10;
    int cooldown_seconds = 5;

    // Workload mix (percentages)
    int read_percent = 80;
    int write_percent = 20;

    // Resource limits
    int max_memory_mb = 1024;
    int max_cpu_percent = 80;
};

struct StressTestConfig {
    StressTestType stress_type = StressTestType::MIXED;
    int duration_seconds = 3600;                // 1 hour default
    int target_cpu_percent = 80;
    int target_memory_percent = 80;
    int target_io_mbps = 100;
    bool allow_oom = false;
    bool allow_crash = false;
};

struct SecurityTestConfig {
    SecurityTestType test_type;
    std::string target_host = "localhost";
    int target_port = 3092;
    Protocol target_protocol = Protocol::NATIVE;

    // Attack parameters
    int attack_duration_seconds = 60;
    int threads = 4;
    bool aggressive_mode = false;

    // Fuzzing parameters
    int fuzz_iterations = 10000;
    int max_payload_size = 65536;

    // Reporting
    bool detailed_logging = true;
    std::string output_file;
};

struct BenchmarkConfig {
    // Test parameters
    std::string benchmark_name;
    int scale_factor = 1;                       // TPC scale factor
    int clients = 1;
    int threads = 1;

    // Duration
    int duration_seconds = 60;
    int warmup_seconds = 10;

    // Targets
    double target_tps = 0;                      // 0 = no limit
    double target_latency_ms = 0;               // 0 = no target

    // Data collection
    int sample_interval_ms = 100;
    bool collect_latency_histogram = true;
    bool collect_cpu_usage = true;
    bool collect_memory_usage = true;
};

//=============================================================================
// Test Results
//=============================================================================

struct TestResult {
    std::string test_id;
    std::string test_name;
    TestStatus status = TestStatus::PENDING;
    TestCategory category;
    TestPriority priority;

    // Timing
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::chrono::milliseconds duration{0};

    // Results
    std::string message;
    std::string error_details;
    std::string stack_trace;

    // Assertions
    int assertions_passed = 0;
    int assertions_failed = 0;

    // Metadata
    std::map<std::string, std::string> metadata;
    std::vector<std::string> logs;
};

struct LatencyStats {
    double min_ms = 0;
    double max_ms = 0;
    double mean_ms = 0;
    double median_ms = 0;      // p50
    double p90_ms = 0;
    double p95_ms = 0;
    double p99_ms = 0;
    double p999_ms = 0;
    double stddev_ms = 0;
    int sample_count = 0;

    // Histogram buckets (microseconds)
    std::map<int, int> histogram;  // bucket_us -> count
};

struct ThroughputStats {
    double total_operations = 0;
    double operations_per_second = 0;
    double bytes_per_second = 0;
    double rows_per_second = 0;
    double transactions_per_second = 0;

    // Time series data
    std::vector<std::pair<double, double>> time_series;  // timestamp, value
};

struct ResourceUsage {
    // CPU
    double cpu_user_percent = 0;
    double cpu_system_percent = 0;
    double cpu_total_percent = 0;

    // Memory
    size_t memory_rss_bytes = 0;
    size_t memory_vss_bytes = 0;
    size_t memory_shared_bytes = 0;

    // I/O
    size_t io_read_bytes = 0;
    size_t io_write_bytes = 0;
    double io_read_mbps = 0;
    double io_write_mbps = 0;

    // Network
    size_t net_rx_bytes = 0;
    size_t net_tx_bytes = 0;
    int connections_active = 0;
    int connections_total = 0;
};

struct LoadTestResult {
    std::string test_id;
    LoadTestType type;
    TestStatus status = TestStatus::PENDING;

    // Configuration
    LoadTestConfig config;

    // Timing
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::chrono::seconds duration{0};

    // Connection metrics
    int total_connections = 0;
    int successful_connections = 0;
    int failed_connections = 0;
    LatencyStats connection_latency;

    // Query metrics
    int64_t total_queries = 0;
    int64_t successful_queries = 0;
    int64_t failed_queries = 0;
    LatencyStats query_latency;
    ThroughputStats query_throughput;

    // Transaction metrics
    int64_t total_transactions = 0;
    int64_t committed_transactions = 0;
    int64_t rolled_back_transactions = 0;
    ThroughputStats transaction_throughput;

    // Resource usage over time
    std::vector<std::pair<double, ResourceUsage>> resource_samples;

    // Errors
    std::map<std::string, int> error_counts;
    std::vector<std::string> error_samples;

    // Target comparison
    bool met_tps_target = false;
    bool met_latency_target = false;
    std::string summary;
};

struct StressTestResult {
    std::string test_id;
    StressTestType type;
    TestStatus status = TestStatus::PENDING;

    // Configuration
    StressTestConfig config;

    // Duration
    std::chrono::seconds actual_duration{0};
    std::chrono::seconds target_duration{0};

    // Stability metrics
    bool experienced_oom = false;
    bool experienced_crash = false;
    bool experienced_hang = false;
    int connection_drops = 0;
    int query_failures = 0;

    // Peak resource usage
    ResourceUsage peak_usage;
    ResourceUsage avg_usage;

    // Degradation metrics
    double latency_degradation_percent = 0;
    double throughput_degradation_percent = 0;

    // Recovery
    bool recovered_after_stress = false;
    std::chrono::seconds recovery_time{0};

    std::string summary;
};

struct SecurityFinding {
    std::string finding_id;
    SecurityTestType test_type;
    std::string test_case;

    enum class Severity {
        CRITICAL,   // Immediate fix required
        HIGH,       // Fix before release
        MEDIUM,     // Should fix
        LOW,        // Informational
        INFO        // Observation
    };
    Severity severity;

    std::string title;
    std::string description;
    std::string evidence;
    std::string reproduction_steps;
    std::string remediation;

    // CVSS-style scoring
    double cvss_score = 0;
    std::string cvss_vector;

    // Status
    bool confirmed = false;
    bool fixed = false;
    std::string fix_commit;
};

struct SecurityTestResult {
    std::string test_id;
    SecurityTestType type;
    TestStatus status = TestStatus::PENDING;

    // Configuration
    SecurityTestConfig config;

    // Findings
    std::vector<SecurityFinding> findings;
    int critical_count = 0;
    int high_count = 0;
    int medium_count = 0;
    int low_count = 0;
    int info_count = 0;

    // Test coverage
    int tests_executed = 0;
    int tests_passed = 0;
    int tests_failed = 0;

    // Fuzzing results (if applicable)
    int64_t fuzz_iterations = 0;
    int crashes_found = 0;
    int hangs_found = 0;
    std::vector<std::string> crash_inputs;

    std::string summary;
};

struct BenchmarkResult {
    std::string benchmark_name;
    TestStatus status = TestStatus::PENDING;

    // Configuration
    BenchmarkConfig config;

    // Timing
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::chrono::seconds duration{0};

    // Primary metrics
    double transactions_per_second = 0;
    double queries_per_second = 0;
    LatencyStats latency;

    // Comparison to targets
    double target_tps = 0;
    double actual_tps = 0;
    double percent_of_target = 0;
    bool met_target = false;

    // Scalability (if multi-config)
    std::map<int, double> tps_by_clients;      // clients -> TPS
    std::map<int, LatencyStats> latency_by_clients;

    // Resource usage
    ResourceUsage peak_usage;
    ResourceUsage avg_usage;

    std::string summary;
};

//=============================================================================
// Test Suite and Report
//=============================================================================

struct TestSuite {
    std::string suite_id;
    std::string suite_name;
    std::string description;
    TestCategory category;

    std::vector<TestResult> results;

    // Summary
    int total_tests = 0;
    int passed = 0;
    int failed = 0;
    int errors = 0;
    int skipped = 0;

    std::chrono::milliseconds total_duration{0};
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;

    double pass_rate() const {
        if (total_tests == 0) return 0;
        return (static_cast<double>(passed) / total_tests) * 100.0;
    }
};

struct TestReport {
    std::string report_id;
    std::string report_name;
    std::string version;
    std::chrono::system_clock::time_point generated_at;

    // Environment
    std::string os_version;
    std::string build_version;
    std::string git_commit;
    std::string hostname;

    // Test suites
    std::vector<TestSuite> suites;

    // Load/stress/security results
    std::vector<LoadTestResult> load_results;
    std::vector<StressTestResult> stress_results;
    std::vector<SecurityTestResult> security_results;
    std::vector<BenchmarkResult> benchmark_results;

    // Overall summary
    int total_tests = 0;
    int total_passed = 0;
    int total_failed = 0;
    int total_errors = 0;
    int total_skipped = 0;

    bool all_passed() const {
        return total_failed == 0 && total_errors == 0;
    }

    double overall_pass_rate() const {
        if (total_tests == 0) return 0;
        return (static_cast<double>(total_passed) / total_tests) * 100.0;
    }
};

//=============================================================================
// Protocol Test Cases
//=============================================================================

struct ProtocolTestCase {
    std::string test_id;            // e.g., "PG-CONN-001"
    std::string name;
    std::string description;
    Protocol protocol;
    TestPriority priority = TestPriority::P1_HIGH;

    // Expected behavior
    std::string expected_result;
    std::vector<std::string> expected_messages;
    std::optional<std::string> expected_error_code;

    // Test parameters
    std::map<std::string, std::string> parameters;

    // Test script (SQL or protocol commands)
    std::string test_script;

    // Validation
    std::function<bool(const std::string&)> validator;
};

struct AuthTestCase {
    std::string test_id;            // e.g., "AUTH-001"
    std::string name;
    std::string description;
    AuthMethod method;
    Protocol protocol = Protocol::NATIVE;
    TestPriority priority = TestPriority::P0_CRITICAL;

    // Credentials
    std::string username;
    std::string password;
    std::optional<std::string> database;

    // Additional auth parameters
    std::map<std::string, std::string> auth_params;

    // Expected result
    bool expect_success;
    std::optional<std::string> expected_error_code;
    std::string expected_behavior;
};

//=============================================================================
// Test Callbacks and Hooks
//=============================================================================

using TestSetupCallback = std::function<bool(const TestConfig&)>;
using TestTeardownCallback = std::function<void()>;
using TestProgressCallback = std::function<void(const std::string& message, double progress)>;
using TestLogCallback = std::function<void(const std::string& level, const std::string& message)>;

struct TestCallbacks {
    TestSetupCallback on_setup;
    TestTeardownCallback on_teardown;
    TestProgressCallback on_progress;
    TestLogCallback on_log;
};

//=============================================================================
// Utility Functions
//=============================================================================

inline std::string toString(TestStatus status) {
    switch (status) {
        case TestStatus::PENDING: return "PENDING";
        case TestStatus::RUNNING: return "RUNNING";
        case TestStatus::PASSED: return "PASSED";
        case TestStatus::FAILED: return "FAILED";
        case TestStatus::ERROR: return "ERROR";
        case TestStatus::SKIPPED: return "SKIPPED";
        case TestStatus::TIMEOUT: return "TIMEOUT";
        case TestStatus::BLOCKED: return "BLOCKED";
        default: return "UNKNOWN";
    }
}

inline std::string toString(TestCategory category) {
    switch (category) {
        case TestCategory::UNIT: return "UNIT";
        case TestCategory::INTEGRATION: return "INTEGRATION";
        case TestCategory::PROTOCOL: return "PROTOCOL";
        case TestCategory::AUTHENTICATION: return "AUTHENTICATION";
        case TestCategory::AUTHORIZATION: return "AUTHORIZATION";
        case TestCategory::LOAD: return "LOAD";
        case TestCategory::STRESS: return "STRESS";
        case TestCategory::ENDURANCE: return "ENDURANCE";
        case TestCategory::SECURITY: return "SECURITY";
        case TestCategory::PERFORMANCE: return "PERFORMANCE";
        case TestCategory::REGRESSION: return "REGRESSION";
        default: return "UNKNOWN";
    }
}

inline std::string toString(Protocol protocol) {
    switch (protocol) {
        case Protocol::POSTGRESQL: return "PostgreSQL";
        case Protocol::MYSQL: return "MySQL";
        case Protocol::TDS: return "TDS";
        case Protocol::FIREBIRD: return "Firebird";
        case Protocol::NATIVE: return "Native";
        default: return "Unknown";
    }
}

inline std::string toString(AuthMethod method) {
    switch (method) {
        case AuthMethod::PASSWORD_PLAIN: return "PASSWORD_PLAIN";
        case AuthMethod::PASSWORD_MD5: return "PASSWORD_MD5";
        case AuthMethod::SCRAM_SHA_256: return "SCRAM_SHA_256";
        case AuthMethod::SCRAM_SHA_512: return "SCRAM_SHA_512";
        case AuthMethod::MYSQL_NATIVE: return "MYSQL_NATIVE";
        case AuthMethod::CACHING_SHA2: return "CACHING_SHA2";
        case AuthMethod::NTLM: return "NTLM";
        case AuthMethod::KERBEROS: return "KERBEROS";
        case AuthMethod::LDAP: return "LDAP";
        case AuthMethod::CERTIFICATE: return "CERTIFICATE";
        case AuthMethod::SAML: return "SAML";
        case AuthMethod::OAUTH2: return "OAUTH2";
        case AuthMethod::MFA_TOTP: return "MFA_TOTP";
        default: return "UNKNOWN";
    }
}

inline std::string toString(SecurityFinding::Severity severity) {
    switch (severity) {
        case SecurityFinding::Severity::CRITICAL: return "CRITICAL";
        case SecurityFinding::Severity::HIGH: return "HIGH";
        case SecurityFinding::Severity::MEDIUM: return "MEDIUM";
        case SecurityFinding::Severity::LOW: return "LOW";
        case SecurityFinding::Severity::INFO: return "INFO";
        default: return "UNKNOWN";
    }
}

inline int getDefaultPort(Protocol protocol) {
    switch (protocol) {
        case Protocol::POSTGRESQL: return 5432;
        case Protocol::MYSQL: return 3306;
        case Protocol::TDS: return 1433;
        case Protocol::FIREBIRD: return 3050;
        case Protocol::NATIVE: return 3092;
        default: return 0;
    }
}

} // namespace testing
} // namespace scratchbird

#endif // SCRATCHBIRD_TESTING_TESTTYPES_H

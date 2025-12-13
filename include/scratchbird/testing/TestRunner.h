/*
 * ScratchBird Database Engine
 * Test Runner - Main Orchestrator
 * Copyright (c) 2025 ScratchBird Project
 */

#ifndef SCRATCHBIRD_TESTING_TESTRUNNER_H
#define SCRATCHBIRD_TESTING_TESTRUNNER_H

#include "TestTypes.h"
#include "ProtocolTester.h"
#include "AuthTester.h"
#include "LoadTester.h"
#include "SecurityTester.h"
#include "BenchmarkRunner.h"
#include <memory>
#include <vector>
#include <functional>

namespace scratchbird {
namespace testing {

//=============================================================================
// Test Environment
//=============================================================================

struct TestEnvironment {
    // Server configuration
    std::string host = "localhost";
    int port = 3092;
    Protocol protocol = Protocol::NATIVE;
    std::string database = "testdb";
    std::string username = "testuser";
    std::string password = "test123";
    bool ssl_enabled = false;

    // Test data configuration
    std::string dataset = "tiny";       // tiny, small, medium, large, huge
    std::string db_config = "tiny";     // tiny, small, medium, large

    // Environment info
    std::string os_version;
    std::string build_version;
    std::string git_commit;
    std::string hostname;

    // Resource limits
    int max_memory_mb = 1024;
    int max_cpu_percent = 80;
    int max_duration_seconds = 3600;
};

//=============================================================================
// Test Filter
//=============================================================================

struct TestFilter {
    // Include filters (empty = include all)
    std::vector<std::string> include_ids;
    std::vector<std::string> include_tags;
    std::vector<TestCategory> include_categories;
    std::vector<TestPriority> include_priorities;
    std::vector<Protocol> include_protocols;

    // Exclude filters
    std::vector<std::string> exclude_ids;
    std::vector<std::string> exclude_tags;
    std::vector<TestCategory> exclude_categories;

    // Match patterns
    std::string name_pattern;           // Regex pattern for test names

    // Helper methods
    bool matches(const TestResult& test) const;
    bool matches(const ProtocolTestCase& test) const;
    bool matches(const AuthTestCase& test) const;
    bool matches(const SecurityTestCase& test) const;
    bool matches(const Benchmark& test) const;
};

//=============================================================================
// Test Runner Options
//=============================================================================

struct TestRunnerOptions {
    // Execution options
    bool parallel_execution = true;
    int max_parallel_tests = 4;
    bool stop_on_failure = false;
    int retry_failed = 0;
    bool shuffle_tests = false;
    uint64_t shuffle_seed = 0;

    // Timeout settings
    int default_timeout_ms = 30000;
    int load_test_timeout_ms = 600000;      // 10 minutes
    int stress_test_timeout_ms = 14400000;  // 4 hours

    // Output options
    bool verbose = false;
    bool quiet = false;
    bool color_output = true;
    bool show_progress = true;

    // Report options
    std::string output_dir = "./test-results";
    bool generate_junit_xml = true;
    bool generate_json = true;
    bool generate_html = true;
    bool generate_csv = false;

    // Database options
    bool create_database = true;
    bool cleanup_database = true;
    bool create_test_data = true;
};

//=============================================================================
// Test Runner
//=============================================================================

class TestRunner {
public:
    TestRunner();
    ~TestRunner();

    // Non-copyable
    TestRunner(const TestRunner&) = delete;
    TestRunner& operator=(const TestRunner&) = delete;

    //=========================================================================
    // Configuration
    //=========================================================================

    void setEnvironment(const TestEnvironment& env);
    TestEnvironment getEnvironment() const;

    void setOptions(const TestRunnerOptions& options);
    TestRunnerOptions getOptions() const;

    void setFilter(const TestFilter& filter);
    TestFilter getFilter() const;

    //=========================================================================
    // Test Discovery
    //=========================================================================

    // Discover all available tests
    std::vector<std::string> discoverTests();

    // Get test counts by category
    std::map<TestCategory, int> getTestCountsByCategory() const;
    std::map<Protocol, int> getTestCountsByProtocol() const;
    std::map<TestPriority, int> getTestCountsByPriority() const;

    //=========================================================================
    // Test Execution
    //=========================================================================

    // Run all tests (respects filters)
    TestReport runAllTests();

    // Run by category
    TestReport runUnitTests();
    TestReport runIntegrationTests();
    TestReport runProtocolTests();
    TestReport runAuthenticationTests();
    TestReport runLoadTests();
    TestReport runStressTests();
    TestReport runEnduranceTests();
    TestReport runSecurityTests();
    TestReport runPerformanceTests();

    // Run by priority
    TestReport runP0Tests();  // Critical
    TestReport runP1Tests();  // High
    TestReport runP2Tests();  // Medium
    TestReport runP3Tests();  // Low

    // Run by protocol
    TestReport runPostgreSQLTests();
    TestReport runMySQLTests();
    TestReport runTDSTests();
    TestReport runFirebirdTests();
    TestReport runNativeTests();

    // Run specific test suites
    TestSuite runProtocolTestSuite(Protocol protocol);
    TestSuite runAuthTestSuite(AuthMethod method);
    LoadTestResult runLoadTestSuite(LoadTestType type);
    StressTestResult runStressTestSuite(StressTestType type);
    SecurityTestResult runSecurityTestSuite(SecurityTestType type);

    // Run individual tests
    TestResult runTest(const std::string& test_id);
    std::vector<TestResult> runTests(const std::vector<std::string>& test_ids);

    //=========================================================================
    // Component Access
    //=========================================================================

    ProtocolTester& getProtocolTester(Protocol protocol);
    AuthTester& getAuthTester();
    LoadTester& getLoadTester();
    SecurityTester& getSecurityTester();
    BenchmarkRunner& getBenchmarkRunner();

    //=========================================================================
    // Test Database Management
    //=========================================================================

    bool createTestDatabase();
    bool loadTestData(const std::string& dataset);
    bool cleanupTestDatabase();
    bool resetTestDatabase();

    //=========================================================================
    // Results and Reporting
    //=========================================================================

    TestReport getLastReport() const;

    // Report generation
    void generateReports();
    void generateJUnitXML(const std::string& path);
    void generateJSON(const std::string& path);
    void generateHTML(const std::string& path);
    void generateCSV(const std::string& path);

    // Console output
    void printSummary() const;
    void printFailedTests() const;
    void printSlowTests(int threshold_ms = 1000) const;

    //=========================================================================
    // Progress and Callbacks
    //=========================================================================

    void setLogCallback(TestLogCallback callback);
    void setProgressCallback(TestProgressCallback callback);

    // Test lifecycle callbacks
    using TestStartCallback = std::function<void(const std::string& test_id)>;
    using TestEndCallback = std::function<void(const TestResult& result)>;
    void setTestStartCallback(TestStartCallback callback);
    void setTestEndCallback(TestEndCallback callback);

    // Suite lifecycle callbacks
    using SuiteStartCallback = std::function<void(const std::string& suite_name)>;
    using SuiteEndCallback = std::function<void(const TestSuite& suite)>;
    void setSuiteStartCallback(SuiteStartCallback callback);
    void setSuiteEndCallback(SuiteEndCallback callback);

    //=========================================================================
    // Control
    //=========================================================================

    void abort();
    bool isRunning() const;
    double getProgress() const;

    //=========================================================================
    // Error Handling
    //=========================================================================

    std::string getLastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void log(const std::string& level, const std::string& message);
    void setError(const std::string& error);
};

//=============================================================================
// Test Data Generator
//=============================================================================

class TestDataGenerator {
public:
    TestDataGenerator();
    ~TestDataGenerator();

    // Configuration
    void setHost(const std::string& host);
    void setPort(int port);
    void setProtocol(Protocol protocol);
    void setDatabase(const std::string& database);
    void setUsername(const std::string& username);
    void setPassword(const std::string& password);

    // Generate test schema
    bool createTestSchema();
    bool dropTestSchema();

    // Generate test data
    bool generateDataset(const std::string& name);  // tiny, small, medium, large, huge

    // Standard dataset specs
    struct DatasetSpec {
        std::string name;
        int64_t row_count;
        int table_count;
        bool include_all_types;
        bool include_indexes;
        bool include_constraints;
    };

    static DatasetSpec getTinySpec();    // 10K rows, 1 MB
    static DatasetSpec getSmallSpec();   // 1M rows, 100 MB
    static DatasetSpec getMediumSpec();  // 10M rows, 1 GB
    static DatasetSpec getLargeSpec();   // 100M rows, 10 GB
    static DatasetSpec getHugeSpec();    // 1B rows, 100 GB

    bool generateCustomDataset(const DatasetSpec& spec);

    // Specific data generators
    bool generateIntegerData(const std::string& table, int64_t rows);
    bool generateTextData(const std::string& table, int64_t rows, int avg_length);
    bool generateAllTypesData(const std::string& table, int64_t rows);
    bool generateRelationalData(int64_t base_rows, int tables);

    // Cleanup
    bool cleanup();

    // Progress callback
    void setProgressCallback(TestProgressCallback callback);

    // Error handling
    std::string getLastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//=============================================================================
// Utility Functions
//=============================================================================

// Parse command line arguments for test runner
TestRunnerOptions parseCommandLine(int argc, char* argv[]);

// Format test duration for display
std::string formatDuration(std::chrono::milliseconds duration);

// Generate test report filename
std::string generateReportFilename(const std::string& prefix, const std::string& extension);

// Color output helpers
std::string colorize(const std::string& text, const std::string& color);
std::string colorStatus(TestStatus status);

} // namespace testing
} // namespace scratchbird

#endif // SCRATCHBIRD_TESTING_TESTRUNNER_H

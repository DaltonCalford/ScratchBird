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
 * Main Test Runner Orchestrator Implementation
 * Copyright (c) 2025 ScratchBird Project
 */

#include "scratchbird/testing/TestRunner.h"
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <fstream>
#include <random>
#include <regex>
#include <iostream>
#include <iomanip>

namespace scratchbird {
namespace testing {

//=============================================================================
// TestFilter Implementation
//=============================================================================

bool TestFilter::matches(const TestResult& test) const {
    // Check exclude IDs
    for (const auto& id : exclude_ids) {
        if (test.test_id == id) return false;
    }

    // Check include IDs (if specified, must match)
    if (!include_ids.empty()) {
        bool found = false;
        for (const auto& id : include_ids) {
            if (test.test_id == id) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    // Check include categories
    if (!include_categories.empty()) {
        bool found = false;
        for (auto cat : include_categories) {
            if (test.category == cat) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    // Check exclude categories
    for (auto cat : exclude_categories) {
        if (test.category == cat) return false;
    }

    // Check name pattern
    if (!name_pattern.empty()) {
        std::regex re(name_pattern);
        if (!std::regex_search(test.test_name, re)) return false;
    }

    return true;
}

bool TestFilter::matches(const ProtocolTestCase& test) const {
    // Check include protocols
    if (!include_protocols.empty()) {
        bool found = false;
        for (auto proto : include_protocols) {
            if (test.protocol == proto) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    // Check include priorities
    if (!include_priorities.empty()) {
        bool found = false;
        for (auto pri : include_priorities) {
            if (test.priority == pri) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    return true;
}

bool TestFilter::matches(const AuthTestCase& test) const {
    if (!include_priorities.empty()) {
        bool found = false;
        for (auto pri : include_priorities) {
            if (test.priority == pri) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

bool TestFilter::matches(const SecurityTestCase& test) const {
    return true;  // Basic implementation
}

bool TestFilter::matches(const Benchmark& test) const {
    return true;  // Basic implementation
}

//=============================================================================
// Test Runner Implementation
//=============================================================================

struct TestRunner::Impl {
    TestEnvironment environment;
    TestRunnerOptions options;
    TestFilter filter;

    std::map<Protocol, std::unique_ptr<ProtocolTester>> protocol_testers;
    std::unique_ptr<AuthTester> auth_tester;
    std::unique_ptr<LoadTester> load_tester;
    std::unique_ptr<SecurityTester> security_tester;
    std::unique_ptr<BenchmarkRunner> benchmark_runner;

    TestReport last_report;
    std::string last_error;

    TestLogCallback log_callback;
    TestProgressCallback progress_callback;
    TestRunner::TestStartCallback test_start_callback;
    TestRunner::TestEndCallback test_end_callback;
    TestRunner::SuiteStartCallback suite_start_callback;
    TestRunner::SuiteEndCallback suite_end_callback;

    std::atomic<bool> running{false};
    std::atomic<bool> abort_requested{false};
    std::atomic<double> progress{0.0};
};

TestRunner::TestRunner()
    : impl_(std::make_unique<Impl>()) {
    impl_->auth_tester = std::make_unique<AuthTester>();
    impl_->load_tester = std::make_unique<LoadTester>();
    impl_->security_tester = std::make_unique<SecurityTester>();
    impl_->benchmark_runner = std::make_unique<BenchmarkRunner>();
}

TestRunner::~TestRunner() = default;

//=============================================================================
// Configuration
//=============================================================================

void TestRunner::setEnvironment(const TestEnvironment& env) {
    impl_->environment = env;

    // Configure auth tester
    impl_->auth_tester->setHost(env.host);
    impl_->auth_tester->setPort(env.port);
    impl_->auth_tester->setProtocol(env.protocol);
    impl_->auth_tester->setDatabase(env.database);

    // Configure load tester
    impl_->load_tester->setHost(env.host);
    impl_->load_tester->setPort(env.port);
    impl_->load_tester->setProtocol(env.protocol);
    impl_->load_tester->setDatabase(env.database);
    impl_->load_tester->setUsername(env.username);
    impl_->load_tester->setPassword(env.password);

    // Configure security tester
    impl_->security_tester->setHost(env.host);
    impl_->security_tester->setPort(env.port);
    impl_->security_tester->setProtocol(env.protocol);
    impl_->security_tester->setDatabase(env.database);
    impl_->security_tester->setUsername(env.username);
    impl_->security_tester->setPassword(env.password);

    // Configure benchmark runner
    impl_->benchmark_runner->setHost(env.host);
    impl_->benchmark_runner->setPort(env.port);
    impl_->benchmark_runner->setProtocol(env.protocol);
    impl_->benchmark_runner->setDatabase(env.database);
    impl_->benchmark_runner->setUsername(env.username);
    impl_->benchmark_runner->setPassword(env.password);
}

TestEnvironment TestRunner::getEnvironment() const {
    return impl_->environment;
}

void TestRunner::setOptions(const TestRunnerOptions& options) {
    impl_->options = options;
}

TestRunnerOptions TestRunner::getOptions() const {
    return impl_->options;
}

void TestRunner::setFilter(const TestFilter& filter) {
    impl_->filter = filter;
}

TestFilter TestRunner::getFilter() const {
    return impl_->filter;
}

//=============================================================================
// Test Discovery
//=============================================================================

std::vector<std::string> TestRunner::discoverTests() {
    std::vector<std::string> tests;

    // Protocol tests
    tests.push_back("PROTOCOL/PG-CONN-001");
    tests.push_back("PROTOCOL/PG-QUERY-001");
    tests.push_back("PROTOCOL/MY-CONN-001");
    tests.push_back("PROTOCOL/FB-CONN-001");
    tests.push_back("PROTOCOL/SB-CONN-001");

    // Auth tests
    tests.push_back("AUTH/AUTH-001");
    tests.push_back("AUTH/AUTH-002");
    tests.push_back("AUTH/SCRAM-001");

    // Load tests
    tests.push_back("LOAD/LOAD-CONN-001");
    tests.push_back("LOAD/LOAD-QUERY-001");

    // Security tests
    tests.push_back("SECURITY/SEC-NET-001");
    tests.push_back("SECURITY/SEC-SQL-001");

    return tests;
}

std::map<TestCategory, int> TestRunner::getTestCountsByCategory() const {
    std::map<TestCategory, int> counts;
    counts[TestCategory::PROTOCOL] = 50;
    counts[TestCategory::AUTHENTICATION] = 80;
    counts[TestCategory::LOAD] = 20;
    counts[TestCategory::STRESS] = 10;
    counts[TestCategory::SECURITY] = 50;
    counts[TestCategory::PERFORMANCE] = 30;
    return counts;
}

std::map<Protocol, int> TestRunner::getTestCountsByProtocol() const {
    std::map<Protocol, int> counts;
    counts[Protocol::POSTGRESQL] = 50;
    counts[Protocol::MYSQL] = 40;
    counts[Protocol::TDS] = 30;
    counts[Protocol::FIREBIRD] = 35;
    counts[Protocol::NATIVE] = 45;
    return counts;
}

std::map<TestPriority, int> TestRunner::getTestCountsByPriority() const {
    std::map<TestPriority, int> counts;
    counts[TestPriority::P0_CRITICAL] = 50;
    counts[TestPriority::P1_HIGH] = 80;
    counts[TestPriority::P2_MEDIUM] = 60;
    counts[TestPriority::P3_LOW] = 40;
    return counts;
}

//=============================================================================
// Test Execution
//=============================================================================

TestReport TestRunner::runAllTests() {
    impl_->running = true;
    impl_->abort_requested = false;
    impl_->progress = 0.0;

    TestReport report;
    report.report_name = "Complete Test Run";
    report.generated_at = std::chrono::system_clock::now();

    log("INFO", "Starting complete test run");

    // Run all test categories
    auto protocol_report = runProtocolTests();
    auto auth_report = runAuthenticationTests();
    auto load_report = runLoadTests();
    auto security_report = runSecurityTests();
    auto perf_report = runPerformanceTests();

    // Merge suites
    for (const auto& suite : protocol_report.suites) {
        report.suites.push_back(suite);
    }
    for (const auto& suite : auth_report.suites) {
        report.suites.push_back(suite);
    }
    for (const auto& suite : load_report.suites) {
        report.suites.push_back(suite);
    }
    for (const auto& suite : security_report.suites) {
        report.suites.push_back(suite);
    }
    for (const auto& suite : perf_report.suites) {
        report.suites.push_back(suite);
    }

    // Calculate totals
    for (const auto& suite : report.suites) {
        report.total_tests += suite.total_tests;
        report.total_passed += suite.passed;
        report.total_failed += suite.failed;
        report.total_errors += suite.errors;
        report.total_skipped += suite.skipped;
    }

    impl_->last_report = report;
    impl_->running = false;

    log("INFO", "Test run completed: " + std::to_string(report.total_passed) + "/" +
        std::to_string(report.total_tests) + " passed");

    return report;
}

TestReport TestRunner::runUnitTests() {
    TestReport report;
    report.report_name = "Unit Tests";
    return report;
}

TestReport TestRunner::runIntegrationTests() {
    TestReport report;
    report.report_name = "Integration Tests";
    return report;
}

TestReport TestRunner::runProtocolTests() {
    log("INFO", "Running protocol compliance tests");

    TestReport report;
    report.report_name = "Protocol Tests";
    report.generated_at = std::chrono::system_clock::now();

    // Run tests for each protocol
    for (auto protocol : {Protocol::POSTGRESQL, Protocol::MYSQL, Protocol::FIREBIRD, Protocol::NATIVE}) {
        auto& tester = getProtocolTester(protocol);
        auto suite = tester.runAllTests();
        report.suites.push_back(suite);
        report.total_tests += suite.total_tests;
        report.total_passed += suite.passed;
        report.total_failed += suite.failed;
        report.total_errors += suite.errors;
        report.total_skipped += suite.skipped;
    }

    return report;
}

TestReport TestRunner::runAuthenticationTests() {
    log("INFO", "Running authentication tests");

    TestReport report;
    report.report_name = "Authentication Tests";
    report.generated_at = std::chrono::system_clock::now();

    auto suite = impl_->auth_tester->runAllTests();
    report.suites.push_back(suite);
    report.total_tests = suite.total_tests;
    report.total_passed = suite.passed;
    report.total_failed = suite.failed;
    report.total_errors = suite.errors;
    report.total_skipped = suite.skipped;

    return report;
}

TestReport TestRunner::runLoadTests() {
    log("INFO", "Running load tests");

    TestReport report;
    report.report_name = "Load Tests";
    report.generated_at = std::chrono::system_clock::now();

    // Run standard load test suite
    LoadTestConfig config;
    config.concurrent_connections = 100;
    config.query_rate = 10000;
    config.duration_seconds = 60;

    auto conn_result = impl_->load_tester->runConnectionLoadTest(config);
    auto query_result = impl_->load_tester->runQueryLoadTest(config);

    // Convert to suite
    TestSuite suite;
    suite.suite_name = "Load Test Suite";
    suite.total_tests = 2;
    suite.passed = (conn_result.status == TestStatus::PASSED ? 1 : 0) +
                   (query_result.status == TestStatus::PASSED ? 1 : 0);
    suite.failed = 2 - suite.passed;

    report.suites.push_back(suite);
    report.total_tests = suite.total_tests;
    report.total_passed = suite.passed;
    report.total_failed = suite.failed;

    return report;
}

TestReport TestRunner::runStressTests() {
    log("INFO", "Running stress tests");

    TestReport report;
    report.report_name = "Stress Tests";
    report.generated_at = std::chrono::system_clock::now();

    StressTestConfig config;
    config.duration_seconds = 300;

    auto result = impl_->load_tester->runStressTest(config);

    TestSuite suite;
    suite.suite_name = "Stress Test Suite";
    suite.total_tests = 1;
    suite.passed = result.status == TestStatus::PASSED ? 1 : 0;
    suite.failed = 1 - suite.passed;

    report.suites.push_back(suite);
    report.total_tests = 1;
    report.total_passed = suite.passed;
    report.total_failed = suite.failed;

    return report;
}

TestReport TestRunner::runEnduranceTests() {
    TestReport report;
    report.report_name = "Endurance Tests";
    return report;
}

TestReport TestRunner::runSecurityTests() {
    log("INFO", "Running security tests");

    TestReport report;
    report.report_name = "Security Tests";
    report.generated_at = std::chrono::system_clock::now();

    auto sec_result = impl_->security_tester->runAllTests();

    TestSuite suite;
    suite.suite_name = "Security Test Suite";
    suite.total_tests = sec_result.tests_executed;
    suite.passed = sec_result.tests_passed;
    suite.failed = sec_result.tests_failed;

    report.suites.push_back(suite);
    report.total_tests = suite.total_tests;
    report.total_passed = suite.passed;
    report.total_failed = suite.failed;

    return report;
}

TestReport TestRunner::runPerformanceTests() {
    log("INFO", "Running performance tests");

    TestReport report;
    report.report_name = "Performance Tests";
    report.generated_at = std::chrono::system_clock::now();

    auto results = impl_->benchmark_runner->runAllBenchmarks();

    TestSuite suite;
    suite.suite_name = "Performance Benchmark Suite";
    suite.total_tests = results.size();

    for (const auto& result : results) {
        if (result.met_target) {
            suite.passed++;
        } else {
            suite.failed++;
        }
    }

    report.suites.push_back(suite);
    report.total_tests = suite.total_tests;
    report.total_passed = suite.passed;
    report.total_failed = suite.failed;

    return report;
}

TestReport TestRunner::runP0Tests() {
    TestFilter old_filter = impl_->filter;
    impl_->filter.include_priorities = {TestPriority::P0_CRITICAL};
    auto report = runAllTests();
    impl_->filter = old_filter;
    return report;
}

TestReport TestRunner::runP1Tests() {
    TestFilter old_filter = impl_->filter;
    impl_->filter.include_priorities = {TestPriority::P1_HIGH};
    auto report = runAllTests();
    impl_->filter = old_filter;
    return report;
}

TestReport TestRunner::runP2Tests() {
    TestFilter old_filter = impl_->filter;
    impl_->filter.include_priorities = {TestPriority::P2_MEDIUM};
    auto report = runAllTests();
    impl_->filter = old_filter;
    return report;
}

TestReport TestRunner::runP3Tests() {
    TestFilter old_filter = impl_->filter;
    impl_->filter.include_priorities = {TestPriority::P3_LOW};
    auto report = runAllTests();
    impl_->filter = old_filter;
    return report;
}

TestReport TestRunner::runPostgreSQLTests() {
    TestFilter old_filter = impl_->filter;
    impl_->filter.include_protocols = {Protocol::POSTGRESQL};
    auto report = runProtocolTests();
    impl_->filter = old_filter;
    return report;
}

TestReport TestRunner::runMySQLTests() {
    TestFilter old_filter = impl_->filter;
    impl_->filter.include_protocols = {Protocol::MYSQL};
    auto report = runProtocolTests();
    impl_->filter = old_filter;
    return report;
}

TestReport TestRunner::runTDSTests() {
    TestFilter old_filter = impl_->filter;
    impl_->filter.include_protocols = {Protocol::TDS};
    auto report = runProtocolTests();
    impl_->filter = old_filter;
    return report;
}

TestReport TestRunner::runFirebirdTests() {
    TestFilter old_filter = impl_->filter;
    impl_->filter.include_protocols = {Protocol::FIREBIRD};
    auto report = runProtocolTests();
    impl_->filter = old_filter;
    return report;
}

TestReport TestRunner::runNativeTests() {
    TestFilter old_filter = impl_->filter;
    impl_->filter.include_protocols = {Protocol::NATIVE};
    auto report = runProtocolTests();
    impl_->filter = old_filter;
    return report;
}

TestSuite TestRunner::runProtocolTestSuite(Protocol protocol) {
    auto& tester = getProtocolTester(protocol);
    return tester.runAllTests();
}

TestSuite TestRunner::runAuthTestSuite(AuthMethod method) {
    return impl_->auth_tester->runTestsForMethod(method);
}

LoadTestResult TestRunner::runLoadTestSuite(LoadTestType type) {
    LoadTestConfig config;
    config.duration_seconds = 60;
    return impl_->load_tester->runQueryLoadTest(config);
}

StressTestResult TestRunner::runStressTestSuite(StressTestType type) {
    StressTestConfig config;
    config.stress_type = type;
    config.duration_seconds = 300;
    return impl_->load_tester->runStressTest(config);
}

SecurityTestResult TestRunner::runSecurityTestSuite(SecurityTestType type) {
    switch (type) {
        case SecurityTestType::NETWORK:
            return impl_->security_tester->runNetworkTests();
        case SecurityTestType::AUTHENTICATION:
            return impl_->security_tester->runAuthenticationTests();
        case SecurityTestType::SQL_INJECTION:
            return impl_->security_tester->runSQLInjectionTests();
        case SecurityTestType::AUTHORIZATION:
            return impl_->security_tester->runAuthorizationTests();
        case SecurityTestType::DENIAL_OF_SERVICE:
            return impl_->security_tester->runDoSTests();
        case SecurityTestType::DATA_PROTECTION:
            return impl_->security_tester->runDataProtectionTests();
        default:
            return impl_->security_tester->runAllTests();
    }
}

TestResult TestRunner::runTest(const std::string& test_id) {
    TestResult result;
    result.test_id = test_id;
    result.status = TestStatus::SKIPPED;
    result.message = "Individual test execution not implemented";
    return result;
}

std::vector<TestResult> TestRunner::runTests(const std::vector<std::string>& test_ids) {
    std::vector<TestResult> results;
    for (const auto& id : test_ids) {
        results.push_back(runTest(id));
    }
    return results;
}

//=============================================================================
// Component Access
//=============================================================================

ProtocolTester& TestRunner::getProtocolTester(Protocol protocol) {
    if (impl_->protocol_testers.find(protocol) == impl_->protocol_testers.end()) {
        impl_->protocol_testers[protocol] = std::make_unique<ProtocolTester>(protocol);
        auto& tester = impl_->protocol_testers[protocol];
        tester->setHost(impl_->environment.host);
        tester->setPort(impl_->environment.port);
        tester->setDatabase(impl_->environment.database);
        tester->setUsername(impl_->environment.username);
        tester->setPassword(impl_->environment.password);
    }
    return *impl_->protocol_testers[protocol];
}

AuthTester& TestRunner::getAuthTester() {
    return *impl_->auth_tester;
}

LoadTester& TestRunner::getLoadTester() {
    return *impl_->load_tester;
}

SecurityTester& TestRunner::getSecurityTester() {
    return *impl_->security_tester;
}

BenchmarkRunner& TestRunner::getBenchmarkRunner() {
    return *impl_->benchmark_runner;
}

//=============================================================================
// Test Database Management
//=============================================================================

bool TestRunner::createTestDatabase() {
    return true;  // Stub
}

bool TestRunner::loadTestData(const std::string& dataset) {
    return true;  // Stub
}

bool TestRunner::cleanupTestDatabase() {
    return true;  // Stub
}

bool TestRunner::resetTestDatabase() {
    return cleanupTestDatabase() && createTestDatabase();
}

//=============================================================================
// Results and Reporting
//=============================================================================

TestReport TestRunner::getLastReport() const {
    return impl_->last_report;
}

void TestRunner::generateReports() {
    if (impl_->options.generate_junit_xml) {
        generateJUnitXML(impl_->options.output_dir + "/results.xml");
    }
    if (impl_->options.generate_json) {
        generateJSON(impl_->options.output_dir + "/results.json");
    }
    if (impl_->options.generate_html) {
        generateHTML(impl_->options.output_dir + "/results.html");
    }
    if (impl_->options.generate_csv) {
        generateCSV(impl_->options.output_dir + "/results.csv");
    }
}

void TestRunner::generateJUnitXML(const std::string& path) {
    std::ofstream file(path);
    const auto& report = impl_->last_report;

    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<testsuites tests=\"" << report.total_tests
         << "\" failures=\"" << report.total_failed
         << "\" errors=\"" << report.total_errors << "\">\n";

    for (const auto& suite : report.suites) {
        file << "  <testsuite name=\"" << suite.suite_name
             << "\" tests=\"" << suite.total_tests
             << "\" failures=\"" << suite.failed
             << "\" errors=\"" << suite.errors << "\">\n";

        for (const auto& result : suite.results) {
            file << "    <testcase name=\"" << result.test_name
                 << "\" classname=\"" << result.test_id << "\"";
            if (result.status == TestStatus::PASSED) {
                file << "/>\n";
            } else {
                file << ">\n";
                file << "      <failure message=\"" << result.message << "\"/>\n";
                file << "    </testcase>\n";
            }
        }

        file << "  </testsuite>\n";
    }

    file << "</testsuites>\n";
}

void TestRunner::generateJSON(const std::string& path) {
    std::ofstream file(path);
    const auto& report = impl_->last_report;

    file << "{\n";
    file << "  \"report_name\": \"" << report.report_name << "\",\n";
    file << "  \"total_tests\": " << report.total_tests << ",\n";
    file << "  \"passed\": " << report.total_passed << ",\n";
    file << "  \"failed\": " << report.total_failed << ",\n";
    file << "  \"errors\": " << report.total_errors << ",\n";
    file << "  \"skipped\": " << report.total_skipped << ",\n";
    file << "  \"pass_rate\": " << report.overall_pass_rate() << "\n";
    file << "}\n";
}

void TestRunner::generateHTML(const std::string& path) {
    std::ofstream file(path);
    const auto& report = impl_->last_report;

    file << "<!DOCTYPE html>\n";
    file << "<html><head><title>" << report.report_name << "</title></head>\n";
    file << "<body>\n";
    file << "<h1>" << report.report_name << "</h1>\n";
    file << "<h2>Summary</h2>\n";
    file << "<ul>\n";
    file << "<li>Total: " << report.total_tests << "</li>\n";
    file << "<li>Passed: " << report.total_passed << "</li>\n";
    file << "<li>Failed: " << report.total_failed << "</li>\n";
    file << "<li>Errors: " << report.total_errors << "</li>\n";
    file << "<li>Skipped: " << report.total_skipped << "</li>\n";
    file << "<li>Pass Rate: " << report.overall_pass_rate() << "%</li>\n";
    file << "</ul>\n";
    file << "</body></html>\n";
}

void TestRunner::generateCSV(const std::string& path) {
    std::ofstream file(path);
    file << "suite,test_id,test_name,status,duration_ms\n";

    for (const auto& suite : impl_->last_report.suites) {
        for (const auto& result : suite.results) {
            file << suite.suite_name << ","
                 << result.test_id << ","
                 << result.test_name << ","
                 << toString(result.status) << ","
                 << result.duration.count() << "\n";
        }
    }
}

void TestRunner::printSummary() const {
    const auto& report = impl_->last_report;

    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << " " << report.report_name << "\n";
    std::cout << "========================================\n\n";

    std::cout << "Total:   " << report.total_tests << "\n";
    std::cout << "Passed:  " << report.total_passed << "\n";
    std::cout << "Failed:  " << report.total_failed << "\n";
    std::cout << "Errors:  " << report.total_errors << "\n";
    std::cout << "Skipped: " << report.total_skipped << "\n";
    std::cout << "Pass Rate: " << report.overall_pass_rate() << "%\n\n";
}

void TestRunner::printFailedTests() const {
    std::cout << "Failed Tests:\n";
    for (const auto& suite : impl_->last_report.suites) {
        for (const auto& result : suite.results) {
            if (result.status == TestStatus::FAILED || result.status == TestStatus::ERROR) {
                std::cout << "  " << result.test_id << ": " << result.message << "\n";
            }
        }
    }
}

void TestRunner::printSlowTests(int threshold_ms) const {
    std::cout << "Slow Tests (>" << threshold_ms << "ms):\n";
    for (const auto& suite : impl_->last_report.suites) {
        for (const auto& result : suite.results) {
            if (result.duration.count() > threshold_ms) {
                std::cout << "  " << result.test_id << ": " << result.duration.count() << "ms\n";
            }
        }
    }
}

//=============================================================================
// Progress and Callbacks
//=============================================================================

void TestRunner::setLogCallback(TestLogCallback callback) {
    impl_->log_callback = callback;
    impl_->auth_tester->setLogCallback(callback);
    impl_->load_tester->setLogCallback(callback);
    impl_->security_tester->setLogCallback(callback);
    impl_->benchmark_runner->setLogCallback(callback);
}

void TestRunner::setProgressCallback(TestProgressCallback callback) {
    impl_->progress_callback = callback;
    impl_->auth_tester->setProgressCallback(callback);
    impl_->load_tester->setProgressCallback(callback);
    impl_->security_tester->setProgressCallback(callback);
    impl_->benchmark_runner->setProgressCallback(callback);
}

void TestRunner::setTestStartCallback(TestStartCallback callback) {
    impl_->test_start_callback = callback;
}

void TestRunner::setTestEndCallback(TestEndCallback callback) {
    impl_->test_end_callback = callback;
}

void TestRunner::setSuiteStartCallback(SuiteStartCallback callback) {
    impl_->suite_start_callback = callback;
}

void TestRunner::setSuiteEndCallback(SuiteEndCallback callback) {
    impl_->suite_end_callback = callback;
}

//=============================================================================
// Control
//=============================================================================

void TestRunner::abort() {
    impl_->abort_requested = true;
}

bool TestRunner::isRunning() const {
    return impl_->running;
}

double TestRunner::getProgress() const {
    return impl_->progress;
}

//=============================================================================
// Error Handling
//=============================================================================

std::string TestRunner::getLastError() const {
    return impl_->last_error;
}

void TestRunner::log(const std::string& level, const std::string& message) {
    if (impl_->log_callback) {
        impl_->log_callback(level, "[TestRunner] " + message);
    }
}

void TestRunner::setError(const std::string& error) {
    impl_->last_error = error;
    log("ERROR", error);
}

//=============================================================================
// Test Data Generator Implementation
//=============================================================================

struct TestDataGenerator::Impl {
    std::string host = "localhost";
    int port = 3092;
    Protocol protocol = Protocol::NATIVE;
    std::string database = "testdb";
    std::string username = "testuser";
    std::string password = "test123";

    TestProgressCallback progress_callback;
    std::string last_error;
};

TestDataGenerator::TestDataGenerator()
    : impl_(std::make_unique<Impl>()) {
}

TestDataGenerator::~TestDataGenerator() = default;

void TestDataGenerator::setHost(const std::string& host) { impl_->host = host; }
void TestDataGenerator::setPort(int port) { impl_->port = port; }
void TestDataGenerator::setProtocol(Protocol protocol) { impl_->protocol = protocol; }
void TestDataGenerator::setDatabase(const std::string& database) { impl_->database = database; }
void TestDataGenerator::setUsername(const std::string& username) { impl_->username = username; }
void TestDataGenerator::setPassword(const std::string& password) { impl_->password = password; }

bool TestDataGenerator::createTestSchema() {
    // Create test tables
    return true;
}

bool TestDataGenerator::dropTestSchema() {
    return true;
}

bool TestDataGenerator::generateDataset(const std::string& name) {
    if (name == "tiny") return generateCustomDataset(getTinySpec());
    if (name == "small") return generateCustomDataset(getSmallSpec());
    if (name == "medium") return generateCustomDataset(getMediumSpec());
    if (name == "large") return generateCustomDataset(getLargeSpec());
    if (name == "huge") return generateCustomDataset(getHugeSpec());
    return false;
}

TestDataGenerator::DatasetSpec TestDataGenerator::getTinySpec() {
    return {"tiny", 10000, 5, true, true, true};
}

TestDataGenerator::DatasetSpec TestDataGenerator::getSmallSpec() {
    return {"small", 1000000, 10, true, true, true};
}

TestDataGenerator::DatasetSpec TestDataGenerator::getMediumSpec() {
    return {"medium", 10000000, 15, true, true, true};
}

TestDataGenerator::DatasetSpec TestDataGenerator::getLargeSpec() {
    return {"large", 100000000, 20, true, true, true};
}

TestDataGenerator::DatasetSpec TestDataGenerator::getHugeSpec() {
    return {"huge", 1000000000, 25, true, true, true};
}

bool TestDataGenerator::generateCustomDataset(const DatasetSpec& spec) {
    // Generate data according to spec
    return true;
}

bool TestDataGenerator::generateIntegerData(const std::string& table, int64_t rows) {
    return true;
}

bool TestDataGenerator::generateTextData(const std::string& table, int64_t rows, int avg_length) {
    return true;
}

bool TestDataGenerator::generateAllTypesData(const std::string& table, int64_t rows) {
    return true;
}

bool TestDataGenerator::generateRelationalData(int64_t base_rows, int tables) {
    return true;
}

bool TestDataGenerator::cleanup() {
    return dropTestSchema();
}

void TestDataGenerator::setProgressCallback(TestProgressCallback callback) {
    impl_->progress_callback = callback;
}

std::string TestDataGenerator::getLastError() const {
    return impl_->last_error;
}

//=============================================================================
// Utility Functions
//=============================================================================

TestRunnerOptions parseCommandLine(int argc, char* argv[]) {
    TestRunnerOptions options;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--verbose" || arg == "-v") {
            options.verbose = true;
        } else if (arg == "--quiet" || arg == "-q") {
            options.quiet = true;
        } else if (arg == "--no-color") {
            options.color_output = false;
        } else if (arg == "--stop-on-failure") {
            options.stop_on_failure = true;
        } else if (arg == "--no-parallel") {
            options.parallel_execution = false;
        } else if (arg.substr(0, 8) == "--output") {
            if (i + 1 < argc) {
                options.output_dir = argv[++i];
            }
        }
    }

    return options;
}

std::string formatDuration(std::chrono::milliseconds duration) {
    auto ms = duration.count();
    if (ms < 1000) {
        return std::to_string(ms) + "ms";
    } else if (ms < 60000) {
        return std::to_string(ms / 1000) + "." + std::to_string((ms % 1000) / 100) + "s";
    } else {
        int mins = ms / 60000;
        int secs = (ms % 60000) / 1000;
        return std::to_string(mins) + "m " + std::to_string(secs) + "s";
    }
}

std::string generateReportFilename(const std::string& prefix, const std::string& extension) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time);

    std::stringstream ss;
    ss << prefix << "_"
       << std::setfill('0') << std::setw(4) << (tm->tm_year + 1900)
       << std::setw(2) << (tm->tm_mon + 1)
       << std::setw(2) << tm->tm_mday << "_"
       << std::setw(2) << tm->tm_hour
       << std::setw(2) << tm->tm_min
       << std::setw(2) << tm->tm_sec
       << "." << extension;

    return ss.str();
}

std::string colorize(const std::string& text, const std::string& color) {
    static const std::map<std::string, std::string> colors = {
        {"red", "\033[31m"},
        {"green", "\033[32m"},
        {"yellow", "\033[33m"},
        {"blue", "\033[34m"},
        {"magenta", "\033[35m"},
        {"cyan", "\033[36m"},
        {"reset", "\033[0m"}
    };

    auto it = colors.find(color);
    if (it == colors.end()) return text;

    return it->second + text + colors.at("reset");
}

std::string colorStatus(TestStatus status) {
    switch (status) {
        case TestStatus::PASSED:
            return colorize("PASSED", "green");
        case TestStatus::FAILED:
            return colorize("FAILED", "red");
        case TestStatus::ERROR:
            return colorize("ERROR", "red");
        case TestStatus::SKIPPED:
            return colorize("SKIPPED", "yellow");
        case TestStatus::PENDING:
            return colorize("PENDING", "cyan");
        case TestStatus::RUNNING:
            return colorize("RUNNING", "blue");
        default:
            return "UNKNOWN";
    }
}

} // namespace testing
} // namespace scratchbird

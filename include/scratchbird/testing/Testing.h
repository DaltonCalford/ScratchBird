/*
 * ScratchBird Database Engine
 * Testing Framework - Unified Header
 * Copyright (c) 2025 ScratchBird Project
 *
 * Include this single header to get the complete testing framework.
 */

#ifndef SCRATCHBIRD_TESTING_TESTING_H
#define SCRATCHBIRD_TESTING_TESTING_H

// Core types and enums
#include "TestTypes.h"

// Test components
#include "ProtocolTester.h"
#include "AuthTester.h"
#include "LoadTester.h"
#include "SecurityTester.h"
#include "BenchmarkRunner.h"

// Main orchestrator
#include "TestRunner.h"

//=============================================================================
// Version Information
//=============================================================================

namespace scratchbird {
namespace testing {

constexpr int TESTING_VERSION_MAJOR = 1;
constexpr int TESTING_VERSION_MINOR = 0;
constexpr int TESTING_VERSION_PATCH = 0;

inline std::string getTestingVersion() {
    return std::to_string(TESTING_VERSION_MAJOR) + "." +
           std::to_string(TESTING_VERSION_MINOR) + "." +
           std::to_string(TESTING_VERSION_PATCH);
}

//=============================================================================
// Quick Start Functions
//=============================================================================

/**
 * Run all tests with default settings.
 * Returns true if all tests passed.
 */
inline bool runAllTests(const std::string& host = "localhost",
                        int port = 3092,
                        const std::string& database = "testdb",
                        const std::string& username = "testuser",
                        const std::string& password = "test123") {
    TestEnvironment env;
    env.host = host;
    env.port = port;
    env.database = database;
    env.username = username;
    env.password = password;

    TestRunner runner;
    runner.setEnvironment(env);

    TestReport report = runner.runAllTests();
    runner.printSummary();

    return report.all_passed();
}

/**
 * Run quick smoke tests to verify basic functionality.
 */
inline bool runSmokeTests(const std::string& host = "localhost",
                          int port = 3092) {
    TestEnvironment env;
    env.host = host;
    env.port = port;

    TestRunnerOptions opts;
    opts.default_timeout_ms = 5000;
    opts.stop_on_failure = true;

    TestFilter filter;
    filter.include_priorities = {TestPriority::P0_CRITICAL};

    TestRunner runner;
    runner.setEnvironment(env);
    runner.setOptions(opts);
    runner.setFilter(filter);

    TestReport report = runner.runP0Tests();
    return report.all_passed();
}

/**
 * Run protocol compliance tests for a specific protocol.
 */
inline TestSuite runProtocolComplianceTests(Protocol protocol,
                                            const std::string& host = "localhost",
                                            int port = 0) {
    if (port == 0) {
        port = getDefaultPort(protocol);
    }

    auto tester = createProtocolTester(protocol);
    tester->setHost(host);
    tester->setPort(port);

    return tester->runAllTests();
}

/**
 * Run a quick performance benchmark.
 */
inline BenchmarkResult runQuickBenchmark(const std::string& host = "localhost",
                                         int port = 3092,
                                         const std::string& database = "testdb",
                                         const std::string& username = "testuser",
                                         const std::string& password = "test123") {
    BenchmarkRunner runner;
    runner.setHost(host);
    runner.setPort(port);
    runner.setDatabase(database);
    runner.setUsername(username);
    runner.setPassword(password);

    // Run a quick TPC-B style benchmark
    return runner.benchmarkTPCB(1, 1);  // Scale 1, 1 client
}

/**
 * Run security scan.
 */
inline SecurityTestResult runSecurityScan(const std::string& host = "localhost",
                                          int port = 3092,
                                          Protocol protocol = Protocol::NATIVE,
                                          bool aggressive = false) {
    SecurityTester tester;
    tester.setHost(host);
    tester.setPort(port);
    tester.setProtocol(protocol);
    tester.setAggressiveMode(aggressive);

    return tester.runAllTests();
}

} // namespace testing
} // namespace scratchbird

#endif // SCRATCHBIRD_TESTING_TESTING_H

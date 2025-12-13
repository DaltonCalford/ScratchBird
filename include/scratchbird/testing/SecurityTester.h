/*
 * ScratchBird Database Engine
 * Security Testing Framework
 * Copyright (c) 2025 ScratchBird Project
 */

#ifndef SCRATCHBIRD_TESTING_SECURITYTESTER_H
#define SCRATCHBIRD_TESTING_SECURITYTESTER_H

#include "TestTypes.h"
#include <memory>
#include <vector>
#include <functional>

namespace scratchbird {
namespace testing {

//=============================================================================
// Security Test Case
//=============================================================================

struct SecurityTestCase {
    std::string test_id;            // e.g., "SEC-SQL-001"
    std::string name;
    std::string description;
    SecurityTestType type;
    TestPriority priority = TestPriority::P1_HIGH;

    // Attack parameters
    std::string payload;
    std::map<std::string, std::string> parameters;

    // Expected behavior
    bool should_be_blocked = true;
    std::string expected_response;
    std::optional<std::string> expected_error;

    // Risk information
    SecurityFinding::Severity risk_if_failed;
    std::string cvss_vector;
    std::string remediation;
};

//=============================================================================
// Protocol Fuzzer
//=============================================================================

class ProtocolFuzzer {
public:
    ProtocolFuzzer(Protocol protocol);
    ~ProtocolFuzzer();

    // Configuration
    void setHost(const std::string& host);
    void setPort(int port);
    void setTimeout(int timeout_ms);

    // Fuzzing parameters
    void setIterations(int count);
    void setMaxPayloadSize(int bytes);
    void setSeed(uint64_t seed);

    // Fuzzing strategies
    enum class Strategy {
        RANDOM,             // Random byte generation
        MUTATION,           // Mutate valid packets
        GENERATION,         // Generate from grammar
        DICTIONARY          // Use dictionary of known bad inputs
    };
    void setStrategy(Strategy strategy);

    // Dictionary for dictionary-based fuzzing
    void loadDictionary(const std::string& path);
    void addDictionaryEntry(const std::vector<uint8_t>& entry);

    // Grammar for generation-based fuzzing
    void setGrammar(const std::string& grammar_file);

    // Execution
    void start();
    void stop();
    bool isRunning() const;

    // Results
    SecurityTestResult getResults() const;
    std::vector<std::vector<uint8_t>> getCrashInputs() const;
    std::vector<std::vector<uint8_t>> getHangInputs() const;

    // Callbacks
    using CrashCallback = std::function<void(const std::vector<uint8_t>& input)>;
    using HangCallback = std::function<void(const std::vector<uint8_t>& input)>;
    void setCrashCallback(CrashCallback callback);
    void setHangCallback(HangCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//=============================================================================
// SQL Injection Tester
//=============================================================================

class SQLInjectionTester {
public:
    SQLInjectionTester();
    ~SQLInjectionTester();

    // Configuration
    void setHost(const std::string& host);
    void setPort(int port);
    void setProtocol(Protocol protocol);
    void setDatabase(const std::string& database);
    void setUsername(const std::string& username);
    void setPassword(const std::string& password);

    // Test parameters
    void setTestQuery(const std::string& sql);
    void setInjectionPoint(const std::string& param_name);
    void setTimeout(int timeout_ms);

    // Injection techniques
    enum class Technique {
        CLASSIC,            // ' OR '1'='1
        UNION_BASED,        // UNION SELECT
        BLIND_BOOLEAN,      // Boolean-based blind
        BLIND_TIME,         // Time-based blind
        ERROR_BASED,        // Error-based extraction
        STACKED_QUERIES,    // Multiple queries
        OUT_OF_BAND         // OOB via DNS/HTTP
    };
    void enableTechnique(Technique technique);
    void disableTechnique(Technique technique);
    void enableAllTechniques();

    // Evasion techniques
    void enableCaseVariation(bool enable);
    void enableCommentInjection(bool enable);
    void enableEncodingBypass(bool enable);
    void enableWhitespaceVariation(bool enable);

    // Execution
    SecurityTestResult runAllTests();
    SecurityTestResult runTechnique(Technique technique);

    // Payload generation
    std::vector<std::string> generatePayloads(Technique technique);
    std::vector<std::string> getCustomPayloads() const;
    void addCustomPayload(const std::string& payload);
    void loadPayloadsFromFile(const std::string& path);

    // Results
    std::vector<SecurityFinding> getFindings() const;
    bool isVulnerable() const;

    // Callbacks
    void setLogCallback(TestLogCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//=============================================================================
// Network Security Tester
//=============================================================================

class NetworkSecurityTester {
public:
    NetworkSecurityTester();
    ~NetworkSecurityTester();

    // Configuration
    void setHost(const std::string& host);
    void setPort(int port);
    void setProtocol(Protocol protocol);

    // TLS testing
    SecurityTestResult testTLSVersions();
    SecurityTestResult testCipherSuites();
    SecurityTestResult testCertificateValidation();
    SecurityTestResult testProtocolDowngrade();

    // Network attacks
    SecurityTestResult testReplayAttack();
    SecurityTestResult testMITM();
    SecurityTestResult testDNSRebinding();
    SecurityTestResult testTCPReset();

    // Port scanning and fingerprinting
    SecurityTestResult testPortScan();
    SecurityTestResult testServiceFingerprint();
    SecurityTestResult testVersionDisclosure();

    // Results
    std::vector<SecurityFinding> getFindings() const;

    // Callbacks
    void setLogCallback(TestLogCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//=============================================================================
// Authorization Bypass Tester
//=============================================================================

class AuthorizationBypassTester {
public:
    AuthorizationBypassTester();
    ~AuthorizationBypassTester();

    // Configuration
    void setHost(const std::string& host);
    void setPort(int port);
    void setProtocol(Protocol protocol);
    void setDatabase(const std::string& database);

    // Credentials for different privilege levels
    void setAdminCredentials(const std::string& username, const std::string& password);
    void setUserCredentials(const std::string& username, const std::string& password);
    void setLimitedCredentials(const std::string& username, const std::string& password);

    // Bypass tests
    SecurityTestResult testHorizontalEscalation();
    SecurityTestResult testVerticalEscalation();
    SecurityTestResult testIDOR();
    SecurityTestResult testRoleConfusion();
    SecurityTestResult testRLSBypass();
    SecurityTestResult testSchemaEscape();
    SecurityTestResult testSystemCatalogAccess();
    SecurityTestResult testGRANTAbuse();
    SecurityTestResult testOwnershipTransfer();
    SecurityTestResult testPUBLICRoleAbuse();

    // Results
    std::vector<SecurityFinding> getFindings() const;

    // Callbacks
    void setLogCallback(TestLogCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//=============================================================================
// Denial of Service Tester
//=============================================================================

class DoSTester {
public:
    DoSTester();
    ~DoSTester();

    // Configuration
    void setHost(const std::string& host);
    void setPort(int port);
    void setProtocol(Protocol protocol);
    void setDatabase(const std::string& database);
    void setUsername(const std::string& username);
    void setPassword(const std::string& password);

    // Safety limits
    void setMaxDuration(int seconds);
    void setMaxConnections(int count);
    void setMaxMemoryMB(int mb);

    // DoS tests (with safety controls)
    SecurityTestResult testConnectionExhaustion();
    SecurityTestResult testMemoryExhaustion();
    SecurityTestResult testCPUExhaustion();
    SecurityTestResult testDiskExhaustion();
    SecurityTestResult testSlowQueryAttack();
    SecurityTestResult testLockContention();
    SecurityTestResult testTransactionBomb();
    SecurityTestResult testLargeResultSet();
    SecurityTestResult testCompressionBomb();
    SecurityTestResult testSSLRenegotiation();

    // Recovery verification
    bool verifyRecovery();
    int getRecoveryTimeSeconds() const;

    // Results
    std::vector<SecurityFinding> getFindings() const;

    // Callbacks
    void setLogCallback(TestLogCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//=============================================================================
// Data Protection Tester
//=============================================================================

class DataProtectionTester {
public:
    DataProtectionTester();
    ~DataProtectionTester();

    // Configuration
    void setHost(const std::string& host);
    void setPort(int port);
    void setLogPath(const std::string& path);
    void setDataPath(const std::string& path);

    // Data exposure tests
    SecurityTestResult testSensitiveDataInLogs();
    SecurityTestResult testSensitiveDataInErrors();
    SecurityTestResult testMemoryDumpAnalysis();
    SecurityTestResult testCoreDumpAnalysis();
    SecurityTestResult testTempFileExposure();
    SecurityTestResult testBackupFileExposure();

    // Encryption tests
    SecurityTestResult testDataAtRestEncryption();
    SecurityTestResult testKeyManagement();
    SecurityTestResult testPasswordStorage();
    SecurityTestResult testConnectionStringExposure();

    // Patterns to search for
    void addSensitivePattern(const std::string& name, const std::string& regex);
    void loadPatternsFromFile(const std::string& path);

    // Results
    std::vector<SecurityFinding> getFindings() const;

    // Callbacks
    void setLogCallback(TestLogCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

//=============================================================================
// Security Tester - Orchestrator
//=============================================================================

class SecurityTester {
public:
    SecurityTester();
    ~SecurityTester();

    // Non-copyable
    SecurityTester(const SecurityTester&) = delete;
    SecurityTester& operator=(const SecurityTester&) = delete;

    //=========================================================================
    // Configuration
    //=========================================================================

    void setHost(const std::string& host);
    void setPort(int port);
    void setProtocol(Protocol protocol);
    void setDatabase(const std::string& database);
    void setUsername(const std::string& username);
    void setPassword(const std::string& password);

    // Safety settings
    void setAggressiveMode(bool enabled);
    void setMaxDuration(int seconds);

    //=========================================================================
    // Test Execution
    //=========================================================================

    // Run all security tests
    SecurityTestResult runAllTests();

    // Run by category
    SecurityTestResult runNetworkTests();
    SecurityTestResult runAuthenticationTests();
    SecurityTestResult runSQLInjectionTests();
    SecurityTestResult runFuzzingTests();
    SecurityTestResult runAuthorizationTests();
    SecurityTestResult runDoSTests();
    SecurityTestResult runDataProtectionTests();

    // Run specific test cases
    SecurityTestResult runTest(const SecurityTestCase& test_case);
    std::vector<SecurityTestResult> runTests(const std::vector<SecurityTestCase>& test_cases);

    //=========================================================================
    // Specialized Testers
    //=========================================================================

    ProtocolFuzzer& getFuzzer(Protocol protocol);
    SQLInjectionTester& getSQLInjectionTester();
    NetworkSecurityTester& getNetworkTester();
    AuthorizationBypassTester& getAuthzTester();
    DoSTester& getDoSTester();
    DataProtectionTester& getDataProtectionTester();

    //=========================================================================
    // Results and Reporting
    //=========================================================================

    std::vector<SecurityFinding> getAllFindings() const;
    std::vector<SecurityFinding> getCriticalFindings() const;
    std::vector<SecurityFinding> getHighFindings() const;
    std::vector<SecurityFinding> getMediumFindings() const;
    std::vector<SecurityFinding> getLowFindings() const;

    // Summary
    int getTotalFindingsCount() const;
    int getCriticalCount() const;
    int getHighCount() const;
    int getMediumCount() const;
    int getLowCount() const;
    bool hasBlockingFindings() const;  // Critical or High

    // Reports
    std::string generateTextReport() const;
    std::string generateJSON() const;
    std::string generateSARIF() const;  // Static Analysis Results Interchange Format
    std::string generateHTML() const;

    // Export
    void exportFindings(const std::string& path, const std::string& format = "json");

    //=========================================================================
    // Callbacks
    //=========================================================================

    void setLogCallback(TestLogCallback callback);
    void setProgressCallback(TestProgressCallback callback);
    void setFindingCallback(std::function<void(const SecurityFinding&)> callback);

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
// Standard Test Cases
//=============================================================================

std::vector<SecurityTestCase> getNetworkSecurityTestCases();
std::vector<SecurityTestCase> getAuthenticationAttackTestCases();
std::vector<SecurityTestCase> getSQLInjectionTestCases();
std::vector<SecurityTestCase> getProtocolFuzzingTestCases();
std::vector<SecurityTestCase> getAuthorizationBypassTestCases();
std::vector<SecurityTestCase> getDoSTestCases();
std::vector<SecurityTestCase> getDataProtectionTestCases();

// Get all security test cases
std::vector<SecurityTestCase> getAllSecurityTestCases();

} // namespace testing
} // namespace scratchbird

#endif // SCRATCHBIRD_TESTING_SECURITYTESTER_H

/*
 * ScratchBird Database Engine
 * Protocol Compliance Tester
 * Copyright (c) 2025 ScratchBird Project
 */

#ifndef SCRATCHBIRD_TESTING_PROTOCOLTESTER_H
#define SCRATCHBIRD_TESTING_PROTOCOLTESTER_H

#include "TestTypes.h"
#include <memory>
#include <vector>
#include <functional>

namespace scratchbird {
namespace testing {

//=============================================================================
// Protocol Test Interface
//=============================================================================

class ProtocolTester {
public:
    explicit ProtocolTester(Protocol protocol);
    virtual ~ProtocolTester();

    // Non-copyable
    ProtocolTester(const ProtocolTester&) = delete;
    ProtocolTester& operator=(const ProtocolTester&) = delete;

    //=========================================================================
    // Configuration
    //=========================================================================

    void setHost(const std::string& host);
    void setPort(int port);
    void setDatabase(const std::string& database);
    void setUsername(const std::string& username);
    void setPassword(const std::string& password);
    void setSSL(bool enabled);
    void setTimeout(int timeout_ms);

    //=========================================================================
    // Connection Tests
    //=========================================================================

    // Run all connection tests for this protocol
    TestSuite runConnectionTests();

    // Individual connection test methods
    TestResult testValidConnection();
    TestResult testInvalidPassword();
    TestResult testUnknownUser();
    TestResult testInvalidDatabase();
    TestResult testSSLNegotiation();
    TestResult testCancelRequest();
    TestResult testProtocolVersion();
    TestResult testMaxConnections();
    TestResult testConnectionTimeout();
    TestResult testIdleTimeout();

    //=========================================================================
    // Query Tests
    //=========================================================================

    // Run all query tests for this protocol
    TestSuite runQueryTests();

    // Simple query tests
    TestResult testSimpleSelect();
    TestResult testSimpleInsert();
    TestResult testSimpleUpdate();
    TestResult testSimpleDelete();
    TestResult testMultiStatement();
    TestResult testEmptyQuery();
    TestResult testSyntaxError();
    TestResult testQueryTimeout();
    TestResult testQueryCancel();

    // Extended query tests (if supported)
    TestResult testPreparedStatement();
    TestResult testParameterBinding();
    TestResult testDescribeStatement();
    TestResult testExecuteWithLimit();
    TestResult testBinaryParameters();
    TestResult testBinaryResults();

    //=========================================================================
    // Type Serialization Tests
    //=========================================================================

    // Run all type tests for this protocol
    TestSuite runTypeTests();

    // Type test methods
    TestResult testBooleanType();
    TestResult testSmallIntType();
    TestResult testIntegerType();
    TestResult testBigIntType();
    TestResult testRealType();
    TestResult testDoubleType();
    TestResult testNumericType();
    TestResult testVarcharType();
    TestResult testTextType();
    TestResult testByteaType();
    TestResult testDateType();
    TestResult testTimeType();
    TestResult testTimestampType();
    TestResult testTimestampTZType();
    TestResult testIntervalType();
    TestResult testUUIDType();
    TestResult testJSONType();
    TestResult testArrayType();
    TestResult testNetworkTypes();
    TestResult testRangeTypes();
    TestResult testNullValues();

    //=========================================================================
    // Protocol-Specific Tests
    //=========================================================================

    // PostgreSQL-specific
    TestSuite runPostgreSQLCopyTests();
    TestResult testCopyToStdout();
    TestResult testCopyFromStdin();
    TestResult testCopyBinary();
    TestResult testCopyCSV();
    TestResult testCopyCancel();

    // MySQL-specific
    TestSuite runMySQLSpecificTests();
    TestResult testMySQLPing();
    TestResult testMySQLResetConnection();
    TestResult testMySQLChangeDatabase();
    TestResult testMySQLFieldList();
    TestResult testMySQLLocalInfile();

    // TDS-specific
    TestSuite runTDSSpecificTests();
    TestResult testTDSPrelogin();
    TestResult testTDSLogin7();
    TestResult testTDSRPC();
    TestResult testTDSAttention();
    TestResult testTDSEnvChange();

    // Firebird-specific
    TestSuite runFirebirdSpecificTests();
    TestResult testFirebirdAttach();
    TestResult testFirebirdDetach();
    TestResult testFirebirdAllocateStatement();
    TestResult testFirebirdBlobHandling();
    TestResult testFirebirdArrayHandling();

    // Native-specific
    TestSuite runNativeSpecificTests();
    TestResult testNativeStartup();
    TestResult testNativeClusterAuth();
    TestResult testNativeCompression();
    TestResult testNativeHeartbeat();
    TestResult testNativeSBLRTransmission();
    TestResult testNativeFederatedQuery();
    TestResult testNativePubSub();

    //=========================================================================
    // Bulk Test Execution
    //=========================================================================

    // Run all tests for this protocol
    TestSuite runAllTests();

    // Run tests by category
    TestSuite runTestsByPriority(TestPriority priority);
    TestSuite runTestsByCategory(TestCategory category);

    // Run specific test cases
    TestResult runTest(const ProtocolTestCase& test_case);
    std::vector<TestResult> runTests(const std::vector<ProtocolTestCase>& test_cases);

    //=========================================================================
    // Test Registration
    //=========================================================================

    // Register custom test cases
    void registerTest(const ProtocolTestCase& test_case);
    void registerTests(const std::vector<ProtocolTestCase>& test_cases);

    // Get registered tests
    std::vector<ProtocolTestCase> getRegisteredTests() const;
    std::vector<ProtocolTestCase> getTestsForProtocol(Protocol protocol) const;

    //=========================================================================
    // Results and Reporting
    //=========================================================================

    TestSuite getLastTestSuite() const;
    std::vector<TestResult> getFailedTests() const;

    // Generate reports
    std::string generateTextReport() const;
    std::string generateJUnitXML() const;
    std::string generateJSON() const;

    //=========================================================================
    // Callbacks
    //=========================================================================

    void setLogCallback(TestLogCallback callback);
    void setProgressCallback(TestProgressCallback callback);

    //=========================================================================
    // Error Handling
    //=========================================================================

    std::string getLastError() const;

protected:
    //=========================================================================
    // Protocol-Specific Implementation (Override in subclasses)
    //=========================================================================

    virtual bool connect();
    virtual void disconnect();
    virtual bool isConnected() const;

    virtual bool sendMessage(const std::vector<uint8_t>& data);
    virtual std::vector<uint8_t> receiveMessage(int timeout_ms = 5000);

    virtual bool executeQuery(const std::string& sql, std::string& result);
    virtual bool prepareStatement(const std::string& sql, const std::string& name);
    virtual bool executeStatement(const std::string& name,
                                   const std::vector<std::string>& params,
                                   std::string& result);

    //=========================================================================
    // Helper Methods
    //=========================================================================

    void log(const std::string& level, const std::string& message);
    void setError(const std::string& error);
    void reportProgress(const std::string& message, double progress);

    TestResult createResult(const std::string& test_id,
                            const std::string& name,
                            TestStatus status,
                            const std::string& message = "");

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Protocol protocol_;
};

//=============================================================================
// Protocol-Specific Tester Classes
//=============================================================================

class PostgreSQLTester : public ProtocolTester {
public:
    PostgreSQLTester();
    ~PostgreSQLTester() override;

protected:
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    bool sendMessage(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> receiveMessage(int timeout_ms) override;
    bool executeQuery(const std::string& sql, std::string& result) override;
    bool prepareStatement(const std::string& sql, const std::string& name) override;
    bool executeStatement(const std::string& name,
                           const std::vector<std::string>& params,
                           std::string& result) override;

private:
    struct PGImpl;
    std::unique_ptr<PGImpl> pg_impl_;
};

class MySQLTester : public ProtocolTester {
public:
    MySQLTester();
    ~MySQLTester() override;

protected:
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    bool sendMessage(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> receiveMessage(int timeout_ms) override;
    bool executeQuery(const std::string& sql, std::string& result) override;
    bool prepareStatement(const std::string& sql, const std::string& name) override;
    bool executeStatement(const std::string& name,
                           const std::vector<std::string>& params,
                           std::string& result) override;

private:
    struct MySQLImpl;
    std::unique_ptr<MySQLImpl> mysql_impl_;
};

class TDSTester : public ProtocolTester {
public:
    TDSTester();
    ~TDSTester() override;

protected:
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    bool sendMessage(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> receiveMessage(int timeout_ms) override;
    bool executeQuery(const std::string& sql, std::string& result) override;
    bool prepareStatement(const std::string& sql, const std::string& name) override;
    bool executeStatement(const std::string& name,
                           const std::vector<std::string>& params,
                           std::string& result) override;

private:
    struct TDSImpl;
    std::unique_ptr<TDSImpl> tds_impl_;
};

class FirebirdTester : public ProtocolTester {
public:
    FirebirdTester();
    ~FirebirdTester() override;

protected:
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    bool sendMessage(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> receiveMessage(int timeout_ms) override;
    bool executeQuery(const std::string& sql, std::string& result) override;
    bool prepareStatement(const std::string& sql, const std::string& name) override;
    bool executeStatement(const std::string& name,
                           const std::vector<std::string>& params,
                           std::string& result) override;

private:
    struct FBImpl;
    std::unique_ptr<FBImpl> fb_impl_;
};

class NativeTester : public ProtocolTester {
public:
    NativeTester();
    ~NativeTester() override;

protected:
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    bool sendMessage(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> receiveMessage(int timeout_ms) override;
    bool executeQuery(const std::string& sql, std::string& result) override;
    bool prepareStatement(const std::string& sql, const std::string& name) override;
    bool executeStatement(const std::string& name,
                           const std::vector<std::string>& params,
                           std::string& result) override;

private:
    struct NativeImpl;
    std::unique_ptr<NativeImpl> native_impl_;
};

//=============================================================================
// Factory Function
//=============================================================================

std::unique_ptr<ProtocolTester> createProtocolTester(Protocol protocol);

//=============================================================================
// Standard Test Cases
//=============================================================================

// Get standard test cases for each protocol
std::vector<ProtocolTestCase> getPostgreSQLConnectionTests();
std::vector<ProtocolTestCase> getPostgreSQLQueryTests();
std::vector<ProtocolTestCase> getPostgreSQLExtendedQueryTests();
std::vector<ProtocolTestCase> getPostgreSQLCopyTests();
std::vector<ProtocolTestCase> getPostgreSQLTypeTests();

std::vector<ProtocolTestCase> getMySQLConnectionTests();
std::vector<ProtocolTestCase> getMySQLQueryTests();

std::vector<ProtocolTestCase> getTDSConnectionTests();
std::vector<ProtocolTestCase> getTDSQueryTests();

std::vector<ProtocolTestCase> getFirebirdConnectionTests();
std::vector<ProtocolTestCase> getFirebirdQueryTests();

std::vector<ProtocolTestCase> getNativeConnectionTests();
std::vector<ProtocolTestCase> getNativeQueryTests();
std::vector<ProtocolTestCase> getNativeTypeTests();

// Get all standard tests for a protocol
std::vector<ProtocolTestCase> getAllTestsForProtocol(Protocol protocol);

} // namespace testing
} // namespace scratchbird

#endif // SCRATCHBIRD_TESTING_PROTOCOLTESTER_H

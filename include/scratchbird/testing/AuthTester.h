/*
 * ScratchBird Database Engine
 * Authentication Test Suite
 * Copyright (c) 2025 ScratchBird Project
 */

#ifndef SCRATCHBIRD_TESTING_AUTHTESTER_H
#define SCRATCHBIRD_TESTING_AUTHTESTER_H

#include "TestTypes.h"
#include <memory>
#include <vector>
#include <functional>

namespace scratchbird {
namespace testing {

//=============================================================================
// Authentication Tester
//=============================================================================

class AuthTester {
public:
    AuthTester();
    ~AuthTester();

    // Non-copyable
    AuthTester(const AuthTester&) = delete;
    AuthTester& operator=(const AuthTester&) = delete;

    //=========================================================================
    // Configuration
    //=========================================================================

    void setHost(const std::string& host);
    void setPort(int port);
    void setProtocol(Protocol protocol);
    void setDatabase(const std::string& database);
    void setTimeout(int timeout_ms);

    // TLS configuration
    void setSSLEnabled(bool enabled);
    void setSSLCertPath(const std::string& path);
    void setSSLKeyPath(const std::string& path);
    void setSSLCAPath(const std::string& path);

    // LDAP configuration
    void setLDAPServer(const std::string& server);
    void setLDAPBaseDN(const std::string& base_dn);
    void setLDAPBindDN(const std::string& bind_dn);
    void setLDAPBindPassword(const std::string& password);

    // Kerberos configuration
    void setKerberosRealm(const std::string& realm);
    void setKerberosPrincipal(const std::string& principal);
    void setKerberosKeytab(const std::string& keytab_path);

    // OAuth configuration
    void setOAuthTokenURL(const std::string& url);
    void setOAuthClientID(const std::string& client_id);
    void setOAuthClientSecret(const std::string& secret);

    //=========================================================================
    // Password Authentication Tests
    //=========================================================================

    TestSuite runPasswordTests();

    TestResult testValidPassword();
    TestResult testInvalidPassword();
    TestResult testEmptyPassword();
    TestResult testUnknownUser();
    TestResult testSpecialCharPassword();
    TestResult testUnicodePassword();
    TestResult testMaxLengthPassword();
    TestResult testPasswordCase();

    //=========================================================================
    // MD5 Authentication Tests
    //=========================================================================

    TestSuite runMD5Tests();

    TestResult testMD5Valid();
    TestResult testMD5Invalid();
    TestResult testMD5Replay();

    //=========================================================================
    // SCRAM Authentication Tests
    //=========================================================================

    TestSuite runSCRAMTests();

    TestResult testSCRAM256Valid();
    TestResult testSCRAM256Invalid();
    TestResult testSCRAM256ChannelBinding();
    TestResult testSCRAM512Valid();
    TestResult testSCRAMNonceReuse();
    TestResult testSCRAMInvalidProof();
    TestResult testSCRAMInvalidVerifier();
    TestResult testSCRAMIterationCount();

    //=========================================================================
    // MySQL Authentication Tests
    //=========================================================================

    TestSuite runMySQLAuthTests();

    TestResult testMySQLNativeValid();
    TestResult testMySQLNativeInvalid();
    TestResult testCachingSHA2Valid();
    TestResult testCachingSHA2Invalid();
    TestResult testMySQLFullHandshake();

    //=========================================================================
    // TLS/Certificate Authentication Tests
    //=========================================================================

    TestSuite runCertificateTests();

    TestResult testValidCertificate();
    TestResult testExpiredCertificate();
    TestResult testRevokedCertificate();
    TestResult testWrongCACertificate();
    TestResult testSelfSignedCertificate();
    TestResult testClientCertRequired();
    TestResult testCertificateCN();
    TestResult testCertificateSAN();

    //=========================================================================
    // LDAP Authentication Tests
    //=========================================================================

    TestSuite runLDAPTests();

    TestResult testLDAPBindValid();
    TestResult testLDAPBindInvalid();
    TestResult testLDAPServerUnreachable();
    TestResult testLDAPSearchFilter();
    TestResult testLDAPGroupMembership();
    TestResult testLDAPNestedGroups();
    TestResult testLDAPTimeout();
    TestResult testLDAPReferral();

    //=========================================================================
    // Kerberos Authentication Tests
    //=========================================================================

    TestSuite runKerberosTests();

    TestResult testKerberosValidTicket();
    TestResult testKerberosExpiredTicket();
    TestResult testKerberosInvalidPrincipal();
    TestResult testKerberosInvalidRealm();
    TestResult testKerberosForwardable();
    TestResult testKerberosRenewal();
    TestResult testKerberosKeytab();
    TestResult testKerberosGSSAPI();

    //=========================================================================
    // OAuth 2.0 Authentication Tests
    //=========================================================================

    TestSuite runOAuthTests();

    TestResult testOAuthValidToken();
    TestResult testOAuthExpiredToken();
    TestResult testOAuthInvalidToken();
    TestResult testOAuthWrongScope();
    TestResult testOAuthRefresh();
    TestResult testOAuthRevoked();
    TestResult testOAuthClientCredentials();
    TestResult testOAuthPKCE();

    //=========================================================================
    // SAML 2.0 Authentication Tests
    //=========================================================================

    TestSuite runSAMLTests();

    TestResult testSAMLValidAssertion();
    TestResult testSAMLExpiredAssertion();
    TestResult testSAMLInvalidSignature();
    TestResult testSAMLWrongAudience();
    TestResult testSAMLReplayAttack();

    //=========================================================================
    // MFA (TOTP) Authentication Tests
    //=========================================================================

    TestSuite runMFATests();

    TestResult testMFAValidTOTP();
    TestResult testMFAInvalidTOTP();
    TestResult testMFAExpiredTOTP();
    TestResult testMFAReplayAttack();
    TestResult testMFASkew();
    TestResult testMFARecoveryCode();
    TestResult testMFAEnrollment();
    TestResult testMFADisable();

    //=========================================================================
    // Authorization Tests
    //=========================================================================

    TestSuite runAuthorizationTests();

    TestResult testSelectOwned();
    TestResult testSelectNoPermission();
    TestResult testInsertNoPermission();
    TestResult testRoleInheritance();
    TestResult testGroupPermission();
    TestResult testRowLevelSecurity();
    TestResult testColumnLevelSecurity();
    TestResult testSchemaPermission();
    TestResult testGrantRevoke();
    TestResult testDenyOverride();

    //=========================================================================
    // Security Attack Tests
    //=========================================================================

    TestSuite runSecurityAttackTests();

    TestResult testBruteForceProtection();
    TestResult testAccountLockout();
    TestResult testTimingAttack();
    TestResult testSessionFixation();
    TestResult testSessionHijacking();
    TestResult testCredentialStuffing();
    TestResult testPasswordSpraying();

    //=========================================================================
    // Bulk Test Execution
    //=========================================================================

    // Run all authentication tests
    TestSuite runAllTests();

    // Run tests by method
    TestSuite runTestsForMethod(AuthMethod method);

    // Run tests by protocol
    TestSuite runTestsForProtocol(Protocol protocol);

    // Run specific test cases
    TestResult runTest(const AuthTestCase& test_case);
    std::vector<TestResult> runTests(const std::vector<AuthTestCase>& test_cases);

    //=========================================================================
    // Test Registration
    //=========================================================================

    void registerTest(const AuthTestCase& test_case);
    void registerTests(const std::vector<AuthTestCase>& test_cases);
    std::vector<AuthTestCase> getRegisteredTests() const;

    //=========================================================================
    // Results and Reporting
    //=========================================================================

    TestSuite getLastTestSuite() const;
    std::vector<TestResult> getFailedTests() const;

    std::string generateTextReport() const;
    std::string generateJUnitXML() const;
    std::string generateJSON() const;

    // Authentication matrix report
    std::string generateAuthMatrix() const;

    //=========================================================================
    // Callbacks
    //=========================================================================

    void setLogCallback(TestLogCallback callback);
    void setProgressCallback(TestProgressCallback callback);

    //=========================================================================
    // Error Handling
    //=========================================================================

    std::string getLastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    void log(const std::string& level, const std::string& message);
    void setError(const std::string& error);

    TestResult createResult(const std::string& test_id,
                            const std::string& name,
                            TestStatus status,
                            const std::string& message = "");

    bool attemptAuth(const std::string& username,
                     const std::string& password,
                     AuthMethod method,
                     std::string& error_message);
};

//=============================================================================
// Standard Test Cases
//=============================================================================

std::vector<AuthTestCase> getPasswordTestCases();
std::vector<AuthTestCase> getMD5TestCases();
std::vector<AuthTestCase> getSCRAMTestCases();
std::vector<AuthTestCase> getMySQLAuthTestCases();
std::vector<AuthTestCase> getCertificateTestCases();
std::vector<AuthTestCase> getLDAPTestCases();
std::vector<AuthTestCase> getKerberosTestCases();
std::vector<AuthTestCase> getOAuthTestCases();
std::vector<AuthTestCase> getSAMLTestCases();
std::vector<AuthTestCase> getMFATestCases();
std::vector<AuthTestCase> getAuthorizationTestCases();
std::vector<AuthTestCase> getSecurityAttackTestCases();

// Get all standard auth test cases
std::vector<AuthTestCase> getAllAuthTestCases();

// Get test cases by priority
std::vector<AuthTestCase> getAuthTestCasesByPriority(TestPriority priority);

} // namespace testing
} // namespace scratchbird

#endif // SCRATCHBIRD_TESTING_AUTHTESTER_H

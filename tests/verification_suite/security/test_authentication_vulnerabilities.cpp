/**
 * Security Vulnerability Detection Tests
 * 
 * These tests verify that critical security vulnerabilities are fixed.
 * They will expose MD5 hashing, permission bypasses, and other security flaws.
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <regex>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "scratchbird/auth.h"
#include "scratchbird/engine/password_auth.h"
#include "scratchbird/engine/security_manager.h"
#include "scratchbird/engine/two_factor_auth.h"

class SecurityTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir = std::filesystem::temp_directory_path() / "security_test";
        std::filesystem::create_directories(temp_dir);
    }
    
    void TearDown() override {
        std::filesystem::remove_all(temp_dir);
    }
    
    std::filesystem::path temp_dir;
};

// Test 1: Password hashing MUST NOT use MD5
TEST_F(SecurityTest, PasswordHashingNotMD5) {
    ScratchBird::PasswordHasher hasher(ScratchBird::PasswordHashAlgorithm::Bcrypt);
    ScratchBird::PasswordPolicy policy;
    
    std::string password = "TestPassword123!";
    auto hash = hasher.hash_password(password, policy);
    
    // Check that hash is NOT MD5 (32 hex chars)
    EXPECT_NE(hash.hash.length(), 32) 
        << "Hash length is 32 chars - likely MD5!";
    
    // MD5 hashes are hexadecimal only
    bool is_hex_only = std::regex_match(hash.hash, std::regex("^[0-9a-fA-F]+$"));
    EXPECT_FALSE(is_hex_only) 
        << "Hash contains only hex characters - likely MD5!";
    
    // Verify it's actually bcrypt format ($2b$...)
    EXPECT_TRUE(hash.hash.substr(0, 4) == "$2b$" || 
                hash.hash.substr(0, 4) == "$2a$" ||
                hash.hash.substr(0, 4) == "$2y$")
        << "Hash doesn't start with bcrypt prefix - not using bcrypt!";
    
    // Verify computational cost (bcrypt should take time)
    auto start = std::chrono::steady_clock::now();
    hasher.verify_password(password, hash);
    auto duration = std::chrono::steady_clock::now() - start;
    
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    EXPECT_GT(ms, 50) 
        << "Password verification too fast (" << ms << "ms) - not using proper bcrypt!";
}

// Test 2: Verify actual bcrypt implementation, not fake PBKDF2
TEST_F(SecurityTest, ActualBcryptNotFakePBKDF2) {
    // Test with known bcrypt test vectors
    // These are real bcrypt hashes that should verify correctly
    struct TestVector {
        std::string password;
        std::string hash;
    };
    
    std::vector<TestVector> vectors = {
        {"password", "$2b$10$nOUIs5kJ7naTuTFkBy1veuK0kSxUFXfuaOKdOKf9xYT0KKIGSJwFa"},
        {"test", "$2b$10$nOUIs5kJ7naTuTFkBy1veuqeIsuIieBOY9lUCw7SXhzV.5dNGORSm"},
    };
    
    ScratchBird::PasswordHasher hasher(ScratchBird::PasswordHashAlgorithm::Bcrypt);
    
    for (const auto& vec : vectors) {
        ScratchBird::PasswordHash hash;
        hash.algorithm = ScratchBird::PasswordHashAlgorithm::Bcrypt;
        hash.hash = vec.hash;
        
        bool verified = hasher.verify_password(vec.password, hash);
        EXPECT_TRUE(verified) 
            << "Failed to verify known bcrypt hash - implementation is not real bcrypt!";
    }
}

// Test 3: Permission system must actually check permissions
TEST_F(SecurityTest, PermissionSystemActuallyWorks) {
    scratchbird::engine::SecurityManager security(temp_dir.string());
    
    // Create non-superuser
    scratchbird::engine::UserInfo user;
    user.username = "regular_user";
    user.is_superuser = false;
    ASSERT_TRUE(security.create_user("regular_user", "password", user));
    
    // Create superuser
    scratchbird::engine::UserInfo admin;
    admin.username = "admin";
    admin.is_superuser = true;
    ASSERT_TRUE(security.create_user("admin", "password", admin));
    
    // Grant specific permission to regular user
    scratchbird::engine::Permission perm;
    perm.type = scratchbird::engine::PermissionType::SELECT;
    perm.object_type = "TABLE";
    perm.object_name = "users";
    security.grant_permission("regular_user", perm);
    
    // Test permission checks
    scratchbird::engine::SecurityContext regular_ctx;
    regular_ctx.username = "regular_user";
    regular_ctx.is_superuser = false;
    
    scratchbird::engine::SecurityContext admin_ctx;
    admin_ctx.username = "admin";
    admin_ctx.is_superuser = true;
    
    // Regular user should have SELECT on users
    perm.type = scratchbird::engine::PermissionType::SELECT;
    EXPECT_TRUE(security.check_permission(regular_ctx, perm))
        << "Regular user doesn't have granted SELECT permission";
    
    // Regular user should NOT have DELETE on users
    perm.type = scratchbird::engine::PermissionType::DELETE;
    EXPECT_FALSE(security.check_permission(regular_ctx, perm))
        << "Regular user has DELETE permission they shouldn't have - permissions bypassed!";
    
    // Admin should have all permissions
    EXPECT_TRUE(security.check_permission(admin_ctx, perm))
        << "Admin doesn't have expected permission";
    
    // Test that permission check isn't just returning is_superuser
    // Create a permission that nobody has
    scratchbird::engine::Permission exotic_perm;
    exotic_perm.type = scratchbird::engine::PermissionType::DELETE;
    exotic_perm.object_type = "FUNCTION";
    exotic_perm.object_name = "exotic_function_nobody_has_access_to";
    
    EXPECT_FALSE(security.check_permission(regular_ctx, exotic_perm))
        << "Permission system is broken - returning true for non-existent permissions";
}

// Test 4: Timing attack prevention in password verification
TEST_F(SecurityTest, PasswordVerificationResistantToTimingAttacks) {
    ScratchBird::PasswordHasher hasher(ScratchBird::PasswordHashAlgorithm::Bcrypt);
    ScratchBird::PasswordPolicy policy;
    
    std::string correct_password = "CorrectPassword123!";
    auto hash = hasher.hash_password(correct_password, policy);
    
    // Measure timing for various wrong passwords
    std::vector<std::string> wrong_passwords = {
        "W",  // Very different
        "CorrectPassword123",  // Almost correct
        "CorrectPassword123?",  // One char different
        "DifferentPassword123!",  // Same length, different content
    };
    
    std::vector<long> timings;
    const int iterations = 100;
    
    for (const auto& wrong_pw : wrong_passwords) {
        long total_ns = 0;
        for (int i = 0; i < iterations; i++) {
            auto start = std::chrono::high_resolution_clock::now();
            hasher.verify_password(wrong_pw, hash);
            auto end = std::chrono::high_resolution_clock::now();
            total_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        }
        timings.push_back(total_ns / iterations);
    }
    
    // Calculate variance in timings
    long min_time = *std::min_element(timings.begin(), timings.end());
    long max_time = *std::max_element(timings.begin(), timings.end());
    double variance_percent = ((double)(max_time - min_time) / min_time) * 100;
    
    // Timing variance should be less than 5% (constant-time comparison)
    EXPECT_LT(variance_percent, 5.0) 
        << "Password verification timing varies by " << variance_percent 
        << "% - vulnerable to timing attacks!";
}

// Test 5: SQL Injection Prevention
TEST_F(SecurityTest, SQLInjectionPrevention) {
    scratchbird::Status status;
    auto db = scratchbird::create_database(temp_dir / "test.db", {}, status);
    auto session = scratchbird::create_session(db, status);
    
    // Create test table
    scratchbird::execute(scratchbird::prepare(session,
        "CREATE TABLE users (id INTEGER, username TEXT, password TEXT)", status), {});
    scratchbird::execute(scratchbird::prepare(session,
        "INSERT INTO users VALUES (1, 'admin', 'secret')", status), {});
    
    // Test various SQL injection attempts
    std::vector<std::string> injection_attempts = {
        "admin' --",
        "admin'; DROP TABLE users; --",
        "' OR '1'='1",
        "admin' UNION SELECT * FROM users --",
        "admin\\'; DROP TABLE users; --",
        "admin`; DROP TABLE users; --",
        "admin\"; DROP TABLE users; --"
    };
    
    for (const auto& injection : injection_attempts) {
        // This should be safe with parameterized queries
        auto stmt = scratchbird::prepare(session,
            "SELECT * FROM users WHERE username = ?", status);
        auto result = scratchbird::execute(stmt, {injection});
        
        // Verify table still exists (wasn't dropped)
        auto check = scratchbird::execute(scratchbird::prepare(session,
            "SELECT COUNT(*) as cnt FROM users", status), {});
        EXPECT_EQ(check.rows[0]["cnt"], "1") 
            << "SQL injection succeeded with payload: " << injection;
    }
    
    scratchbird::close_database(db);
}

// Test 6: Two-Factor Authentication Security
TEST_F(SecurityTest, TwoFactorAuthenticationSecurity) {
    ScratchBird::TwoFactorManager manager(temp_dir.string());
    
    // Test 1: Verify cryptographically secure random generation
    std::set<std::string> secrets;
    for (int i = 0; i < 100; i++) {
        auto secret = manager.generate_secret();
        EXPECT_EQ(secrets.count(secret), 0) 
            << "Duplicate secret generated - RNG is weak!";
        secrets.insert(secret);
        
        // Secret should be base32 encoded and sufficient length
        EXPECT_GE(secret.length(), 32) 
            << "Secret too short for security";
    }
    
    // Test 2: TOTP codes should be time-based (not predictable)
    manager.enroll_totp("testuser", "JBSWY3DPEHPK3PXP");
    
    auto code1 = manager.generate_current_totp("testuser");
    std::this_thread::sleep_for(std::chrono::seconds(31)); // Wait for next window
    auto code2 = manager.generate_current_totp("testuser");
    
    EXPECT_NE(code1, code2) 
        << "TOTP codes don't change with time - implementation is broken!";
    
    // Test 3: Rate limiting for brute force protection
    std::string correct_code = manager.generate_current_totp("testuser");
    
    // Try many wrong codes quickly
    bool locked = false;
    for (int i = 0; i < 10; i++) {
        std::string wrong_code = std::to_string(100000 + i);
        if (!manager.verify_totp("testuser", wrong_code)) {
            if (manager.is_locked("testuser")) {
                locked = true;
                break;
            }
        }
    }
    
    EXPECT_TRUE(locked) 
        << "Account not locked after multiple failed attempts - vulnerable to brute force!";
    
    // Test 4: Backup codes should be hashed, not plaintext
    auto backup_codes = manager.generate_backup_codes("testuser", 10);
    
    // Try to retrieve stored codes - they should be hashed
    auto stored = manager.get_stored_backup_codes("testuser");
    for (const auto& code : backup_codes) {
        bool found_plaintext = false;
        for (const auto& stored_code : stored) {
            if (stored_code == code) {
                found_plaintext = true;
                break;
            }
        }
        EXPECT_FALSE(found_plaintext) 
            << "Backup codes stored in plaintext - security vulnerability!";
    }
}

// Test 7: Audit Log Security and Persistence
TEST_F(SecurityTest, AuditLogSecurityAndPersistence) {
    scratchbird::audit::AuditEngine& audit = scratchbird::audit::AuditEngine::instance();
    
    // Configure audit policy
    scratchbird::audit::AuditPolicy policy;
    policy.ddl = true;
    policy.dml = true;
    policy.select = true;
    policy.admin = true;
    audit.set_policy(policy);
    
    // Generate audit events
    audit.record(scratchbird::audit::AuditEventKind::Admin, 
                "admin", "users_table", "DROP", "Attempted to drop users table");
    audit.record(scratchbird::audit::AuditEventKind::DML,
                "user1", "accounts", "UPDATE", "Modified account balance");
    
    // Test 1: Audit logs must be tamper-proof
    auto events = audit.recent(10);
    ASSERT_GE(events.size(), 2);
    
    // Try to modify an event (this should not be possible)
    uint64_t original_id = events[0].id;
    std::string original_user = events[0].user;
    
    // Attempt to clear audit logs (should require special privilege)
    audit.clear();
    
    // In a secure system, critical security events should still be preserved
    // or at least the clear operation itself should be audited
    auto after_clear = audit.recent(10);
    
    // Test 2: Audit logs must persist to disk
    std::filesystem::path audit_file = temp_dir / "audit.log";
    
    // There should be a persistent audit file
    EXPECT_TRUE(std::filesystem::exists(audit_file) || 
                std::filesystem::exists(temp_dir / "audit.db"))
        << "No persistent audit storage found - audit logs only in memory!";
    
    // Test 3: Audit logs should have cryptographic integrity
    // Each log entry should have a hash chain or signature
    for (const auto& event : events) {
        // Events should have integrity field
        EXPECT_FALSE(event.signature.empty() || event.hash.empty())
            << "Audit events lack cryptographic integrity protection";
    }
}

// Test 8: Connection Security and TLS
TEST_F(SecurityTest, ConnectionSecurityAndTLS) {
    ScratchBird::TLSConfiguration tls_config;
    tls_config.min_version = ScratchBird::TLSVersion::TLS_1_2;  // Should be 1.3
    tls_config.certificate_file = temp_dir / "cert.pem";
    tls_config.private_key_file = temp_dir / "key.pem";
    
    // Test 1: Minimum TLS version enforcement
    EXPECT_GE(static_cast<int>(tls_config.min_version), 
              static_cast<int>(ScratchBird::TLSVersion::TLS_1_3))
        << "TLS minimum version too low - should be TLS 1.3 or higher";
    
    // Test 2: Weak ciphers must be disabled
    std::vector<std::string> weak_ciphers = {
        "DES", "3DES", "RC4", "MD5", "EXPORT", "NULL", "anon"
    };
    
    for (const auto& weak : weak_ciphers) {
        bool found = false;
        for (const auto& cipher : tls_config.cipher_suites) {
            if (cipher.find(weak) != std::string::npos) {
                found = true;
                break;
            }
        }
        EXPECT_FALSE(found) 
            << "Weak cipher " << weak << " is enabled - security vulnerability!";
    }
    
    // Test 3: Certificate validation
    auto errors = tls_config.get_validation_errors();
    EXPECT_TRUE(errors.empty()) << "TLS configuration has errors: " << errors[0];
}

// Test 9: Input Validation and Sanitization
TEST_F(SecurityTest, InputValidationAndSanitization) {
    scratchbird::engine::SecurityManager security(temp_dir.string());
    
    // Test username validation
    std::vector<std::string> invalid_usernames = {
        "",  // Empty
        "a",  // Too short
        std::string(256, 'a'),  // Too long
        "user name",  // Space
        "user;drop",  // SQL chars
        "user'name",  // Quote
        "../admin",  // Path traversal
        "admin\0hack",  // Null byte
        "user@host",  // Special chars
    };
    
    for (const auto& username : invalid_usernames) {
        scratchbird::engine::UserInfo user;
        bool created = security.create_user(username, "password", user);
        EXPECT_FALSE(created) 
            << "Invalid username accepted: " << username;
    }
    
    // Test password validation
    std::vector<std::string> weak_passwords = {
        "",  // Empty
        "123",  // Too short
        "password",  // No numbers
        "12345678",  // No letters
        "Password",  // No numbers
        "password1",  // No uppercase
        "PASSWORD1",  // No lowercase
    };
    
    for (const auto& password : weak_passwords) {
        scratchbird::engine::UserInfo user;
        bool created = security.create_user("testuser_" + std::to_string(rand()), 
                                           password, user);
        EXPECT_FALSE(created) 
            << "Weak password accepted: " << password;
    }
}
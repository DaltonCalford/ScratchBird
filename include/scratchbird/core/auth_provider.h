/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/core/types.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace scratchbird {
namespace security {
    enum class ScramAlgorithm : uint8_t;
}

namespace core {

/**
 * External Authentication Provider Interface
 *
 * Security Phase 3.1 - Infrastructure for Beta (non-local connectivity)
 *
 * This interface defines the contract for external authentication providers
 * (LDAP, Active Directory, OAuth2, SAML, etc.) to be implemented in Beta
 * when network connectivity is available.
 *
 * For Alpha: Only LocalAuthProvider (password-based) is implemented.
 */

// Authentication result status
enum class AuthResult {
    SUCCESS = 0,           // Authentication successful
    INVALID_CREDENTIALS,   // Username/password incorrect
    USER_DISABLED,         // User account is disabled
    USER_LOCKED,           // User account is locked (too many failures)
    USER_EXPIRED,          // User account has expired
    NETWORK_ERROR,         // Network/connection error (Beta feature)
    PROVIDER_ERROR,        // Provider-specific error
    NOT_IMPLEMENTED        // Feature not yet implemented
};

// Authentication provider type
enum class AuthProviderType {
    LOCAL = 0,             // Local password-based (implemented in Alpha)
    LDAP,                  // LDAP authentication (Beta)
    ACTIVE_DIRECTORY,      // Active Directory (Beta)
    OAUTH2,                // OAuth2 (Beta)
    SAML,                  // SAML SSO (Beta)
    KERBEROS,              // Kerberos (Beta)
    EXTERNAL_SCRIPT        // External authentication script (Beta)
};

// User information returned by authentication provider
struct AuthUserInfo {
    ID user_id{};
    std::string username;
    std::string display_name;
    std::string email;
    std::vector<std::string> external_groups;  // Groups from external provider
    std::string external_id;                    // Provider-specific user ID
    bool is_disabled = false;
    bool is_locked = false;
    bool is_superuser = false;
    ID authkey_id{};
};

struct ScramAuthState {
    std::string username;
    security::ScramAlgorithm algorithm;
    uint32_t iterations = 0;
    std::vector<uint8_t> salt;
    std::vector<uint8_t> stored_key;
    std::vector<uint8_t> server_key;
    std::string client_first_bare;
    std::string server_first;
    std::string full_nonce;
    ID user_id{};
    bool user_exists = false;
    bool is_active = true;
    bool is_superuser = false;
};

/**
 * Abstract base class for authentication providers
 *
 * All authentication providers must implement this interface.
 * The catalog manager delegates authentication to the appropriate provider
 * based on user configuration.
 */
class AuthProvider {
public:
    virtual ~AuthProvider() = default;

    /**
     * Authenticate a user with username and password
     *
     * @param username Username to authenticate
     * @param password Password (plaintext - provider handles hashing)
     * @param user_info_out [out] User information if successful
     * @param error_msg_out [out] Error message if authentication fails
     * @return AuthResult status code
     *
     * Thread-safe: Must be safe to call from multiple threads
     */
    virtual AuthResult authenticate(
        const std::string& username,
        const std::string& password,
        AuthUserInfo& user_info_out,
        std::string& error_msg_out) = 0;

    /**
     * Verify if a user exists (without password)
     *
     * @param username Username to check
     * @param user_info_out [out] User information if exists
     * @return true if user exists, false otherwise
     *
     * Used for user enumeration and management operations.
     */
    virtual bool userExists(
        const std::string& username,
        AuthUserInfo& user_info_out) = 0;

    /**
     * Get external groups for a user
     *
     * @param username Username
     * @param groups_out [out] List of group names
     * @return true if successful, false on error
     *
     * Used to synchronize external groups with internal groups.
     * Only applicable to external providers (LDAP, AD, etc.)
     */
    virtual bool getUserGroups(
        const std::string& username,
        std::vector<std::string>& groups_out) = 0;

    /**
     * Get provider type
     */
    virtual AuthProviderType getType() const = 0;

    /**
     * Get provider name (for logging/debugging)
     */
    virtual std::string getName() const = 0;

    /**
     * Test connection to authentication provider
     *
     * @param error_msg_out [out] Error message if test fails
     * @return true if connection successful, false otherwise
     *
     * Used for health checks and diagnostics.
     * For local provider, always returns true.
     */
    virtual bool testConnection(std::string& error_msg_out) = 0;

    /**
     * Authenticate using MD5 challenge-response
     */
    virtual AuthResult authenticateMd5(
        const std::string& username,
        const uint8_t salt[4],
        const std::string& client_response,
        AuthUserInfo& user_info_out,
        std::string& error_msg_out)
    {
        (void)username;
        (void)salt;
        (void)client_response;
        (void)user_info_out;
        error_msg_out = "MD5 authentication not supported";
        return AuthResult::NOT_IMPLEMENTED;
    }

    /**
     * Begin SCRAM authentication (client-first -> server-first)
     */
    virtual AuthResult beginScramAuth(
        const std::string& username,
        const std::string& client_first,
        security::ScramAlgorithm algorithm,
        ScramAuthState& state_out,
        std::string& server_first_out,
        std::string& error_msg_out)
    {
        (void)username;
        (void)client_first;
        (void)algorithm;
        (void)state_out;
        (void)server_first_out;
        error_msg_out = "SCRAM authentication not supported";
        return AuthResult::NOT_IMPLEMENTED;
    }

    /**
     * Finish SCRAM authentication (client-final -> server-final)
     */
    virtual AuthResult finishScramAuth(
        ScramAuthState& state,
        const std::string& client_final,
        AuthUserInfo& user_info_out,
        std::string& server_final_out,
        std::string& error_msg_out)
    {
        (void)state;
        (void)client_final;
        (void)user_info_out;
        (void)server_final_out;
        error_msg_out = "SCRAM authentication not supported";
        return AuthResult::NOT_IMPLEMENTED;
    }
};

/**
 * Local (password-based) authentication provider
 *
 * Implemented in Alpha. Uses BCrypt password hashing.
 * Authenticates against local user database.
 *
 * P0-2: Includes login attempt tracking and account lockout
 */
class LocalAuthProvider : public AuthProvider {
public:
    explicit LocalAuthProvider(class CatalogManager* catalog,
                               class AuditLogger* audit_logger = nullptr);
    ~LocalAuthProvider() override;

    AuthResult authenticate(
        const std::string& username,
        const std::string& password,
        AuthUserInfo& user_info_out,
        std::string& error_msg_out) override;

    AuthResult authenticateMd5(
        const std::string& username,
        const uint8_t salt[4],
        const std::string& client_response,
        AuthUserInfo& user_info_out,
        std::string& error_msg_out) override;

    AuthResult beginScramAuth(
        const std::string& username,
        const std::string& client_first,
        security::ScramAlgorithm algorithm,
        ScramAuthState& state_out,
        std::string& server_first_out,
        std::string& error_msg_out) override;

    AuthResult finishScramAuth(
        ScramAuthState& state,
        const std::string& client_final,
        AuthUserInfo& user_info_out,
        std::string& server_final_out,
        std::string& error_msg_out) override;

    bool userExists(
        const std::string& username,
        AuthUserInfo& user_info_out) override;

    bool getUserGroups(
        const std::string& username,
        std::vector<std::string>& groups_out) override;

    AuthProviderType getType() const override { return AuthProviderType::LOCAL; }
    std::string getName() const override { return "Local"; }

    bool testConnection(std::string& error_msg_out) override {
        return true; // Always connected (local)
    }

    /**
     * Clear login attempt tracking for a user (admin function)
     * P0-2: Account lockout management
     */
    void clearLoginAttempts(const std::string& username);

    /**
     * Get failed attempt count for a user (admin function)
     * P0-2: Account lockout management
     */
    uint32_t getFailedAttemptCount(const std::string& username);

private:
    class CatalogManager* catalog_;
    class LoginAttemptTracker* login_tracker_;  // P0-2: Brute-force protection
    class AuditLogger* audit_logger_;           // P0-3: Security audit logging (non-owning)
};

/**
 * LDAP Authentication Provider (Beta - Infrastructure only)
 *
 * Not implemented in Alpha. Requires network connectivity.
 * Implementation deferred to Beta release.
 */
class LDAPAuthProvider : public AuthProvider {
public:
    struct Config {
        std::string server_uri;       // ldap://hostname:port or ldaps://
        std::string bind_dn;          // Admin bind DN
        std::string bind_password;    // Admin bind password
        std::string user_base_dn;     // Base DN for users
        std::string group_base_dn;    // Base DN for groups
        std::string user_filter;      // LDAP filter for users
        std::string group_filter;     // LDAP filter for groups
        bool use_tls;                 // Use TLS/SSL
        bool verify_cert;             // Verify server certificate
        int timeout_seconds;          // Connection timeout
    };

    explicit LDAPAuthProvider(const Config& config);
    ~LDAPAuthProvider() override;

    AuthResult authenticate(
        const std::string& username,
        const std::string& password,
        AuthUserInfo& user_info_out,
        std::string& error_msg_out) override;

    bool userExists(
        const std::string& username,
        AuthUserInfo& user_info_out) override;

    bool getUserGroups(
        const std::string& username,
        std::vector<std::string>& groups_out) override;

    AuthProviderType getType() const override { return AuthProviderType::LDAP; }
    std::string getName() const override { return "LDAP"; }
    bool testConnection(std::string& error_msg_out) override;

private:
    Config config_;
    // Actual LDAP connection object will be added in Beta
};

/**
 * Active Directory Authentication Provider (Beta - Infrastructure only)
 */
class ActiveDirectoryAuthProvider : public AuthProvider {
public:
    struct Config {
        std::string domain;           // AD domain
        std::string domain_controller; // DC hostname
        std::string bind_user;        // Service account username
        std::string bind_password;    // Service account password
        bool use_kerberos;            // Use Kerberos authentication
        bool use_ssl;                 // Use SSL/TLS
        int timeout_seconds;
    };

    explicit ActiveDirectoryAuthProvider(const Config& config);
    ~ActiveDirectoryAuthProvider() override;

    AuthResult authenticate(
        const std::string& username,
        const std::string& password,
        AuthUserInfo& user_info_out,
        std::string& error_msg_out) override;

    bool userExists(
        const std::string& username,
        AuthUserInfo& user_info_out) override;

    bool getUserGroups(
        const std::string& username,
        std::vector<std::string>& groups_out) override;

    AuthProviderType getType() const override { return AuthProviderType::ACTIVE_DIRECTORY; }
    std::string getName() const override { return "Active Directory"; }
    bool testConnection(std::string& error_msg_out) override;

private:
    Config config_;
    // Actual AD connection object will be added in Beta
};

/**
 * Authentication Provider Factory
 *
 * Creates appropriate authentication provider based on configuration.
 */
class AuthProviderFactory {
public:
    /**
     * Create authentication provider
     *
     * @param type Provider type
     * @param config_json JSON configuration string (provider-specific)
     * @param catalog Catalog manager (for local auth)
     * @return Unique pointer to auth provider, or nullptr on error
     */
    static std::unique_ptr<AuthProvider> create(
        AuthProviderType type,
        const std::string& config_json,
        CatalogManager* catalog,
        class AuditLogger* audit_logger = nullptr);

    /**
     * Get default provider (local auth)
     */
    static std::unique_ptr<AuthProvider> createDefault(CatalogManager* catalog,
                                                       class AuditLogger* audit_logger = nullptr);
};

} // namespace core
} // namespace scratchbird

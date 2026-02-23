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

/**
 * ScratchBird Authentication Method Interface
 *
 * Alpha 3 Phase 3.4: Security Suite
 *
 * Provides pluggable authentication methods:
 * - SCRAM-SHA-256/512
 * - Certificate-based (TLS client certificates)
 * - Password (PBKDF2)
 * - Trust (no authentication)
 * - Reject (always fail)
 */

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <chrono>
#include <map>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

#ifdef _WIN32
    #ifdef ERROR
        #undef ERROR
    #endif
#endif

namespace scratchbird {
namespace security {

// Forward declarations
struct CertificateInfo;

// ============================================================================
// Authentication Types
// ============================================================================

/**
 * Authentication method type
 */
enum class AuthType : uint8_t {
    TRUST = 0,          // No authentication required
    REJECT = 1,         // Always reject
    PASSWORD = 2,       // Plain password (not recommended)
    MD5 = 3,            // MD5 (legacy, deprecated)
    SCRAM_SHA_256 = 4,  // SCRAM-SHA-256 (recommended)
    SCRAM_SHA_512 = 5,  // SCRAM-SHA-512
    CERTIFICATE = 6,    // TLS client certificate
    LDAP = 7,           // LDAP authentication
    KERBEROS = 8,       // Kerberos/GSSAPI
    PEER = 9,           // Unix socket peer credentials
    IDENT = 10,         // Ident protocol
    RADIUS = 11,        // RADIUS
    PAM = 12            // Pluggable Authentication Modules
};

/**
 * Convert auth type to string
 */
const char* authTypeToString(AuthType type);

/**
 * Parse auth type from string
 */
bool parseAuthType(const std::string& str, AuthType& type);

/**
 * Authentication state
 */
enum class AuthState : uint8_t {
    INITIAL = 0,        // Not started
    IN_PROGRESS = 1,    // Multi-step auth in progress
    SUCCESS = 2,        // Authentication succeeded
    FAILURE = 3,        // Authentication failed
    ERROR = 4           // Internal error
};

/**
 * Authentication failure reason
 */
enum class AuthFailReason : uint8_t {
    NONE = 0,
    INVALID_CREDENTIALS = 1,
    USER_NOT_FOUND = 2,
    ACCOUNT_DISABLED = 3,
    ACCOUNT_LOCKED = 4,
    PASSWORD_EXPIRED = 5,
    CERTIFICATE_INVALID = 6,
    CERTIFICATE_EXPIRED = 7,
    CERTIFICATE_REVOKED = 8,
    CERTIFICATE_NOT_TRUSTED = 9,
    PRINCIPAL_MISMATCH = 10,
    PROTOCOL_ERROR = 11,
    TIMEOUT = 12,
    RATE_LIMITED = 13,
    NOT_ALLOWED = 14,       // HBA rule denies
    INTERNAL_ERROR = 15
};

/**
 * Convert failure reason to string
 */
const char* authFailReasonToString(AuthFailReason reason);

// ============================================================================
// Authentication Context
// ============================================================================

/**
 * Connection information for authentication
 */
struct ConnectionInfo {
    // Network info
    std::string client_address;     // Client IP address
    uint16_t client_port = 0;       // Client port
    std::string server_address;     // Server address (for SNI)
    uint16_t server_port = 0;       // Server port

    // Protocol info
    std::string protocol;           // "native", "postgresql", "mysql", "firebird"
    bool is_ssl = false;            // TLS connection
    bool is_unix_socket = false;    // Unix domain socket

    // Database info
    std::string database_name;      // Requested database

    // TLS info (if is_ssl)
    const CertificateInfo* client_cert = nullptr;  // Client certificate
    std::string sni_hostname;       // SNI hostname

    // Unix socket info (if is_unix_socket)
    uint32_t peer_uid = 0;          // Peer UID (OS-mapped)
    uint32_t peer_gid = 0;          // Peer GID (OS-mapped)
    std::string peer_username;      // Peer username (from UID lookup)
};

/**
 * Authentication context
 *
 * Passed through the authentication process to track state.
 */
class AuthContext {
public:
    AuthContext();
    ~AuthContext();

    // Connection info
    void setConnectionInfo(const ConnectionInfo& info);
    const ConnectionInfo& connectionInfo() const { return conn_info_; }

    // Username
    void setUsername(const std::string& username);
    const std::string& username() const { return username_; }

    // Auth method
    void setAuthType(AuthType type);
    AuthType authType() const { return auth_type_; }

    // State
    AuthState state() const { return state_; }
    void setState(AuthState state);

    // Failure info
    AuthFailReason failureReason() const { return fail_reason_; }
    const std::string& failureMessage() const { return fail_message_; }
    void setFailure(AuthFailReason reason, const std::string& message = "");

    // Multi-step auth data
    void setAuthData(const std::string& key, const std::string& value);
    std::string getAuthData(const std::string& key) const;
    bool hasAuthData(const std::string& key) const;
    void clearAuthData();

    // Binary auth data
    void setAuthDataBinary(const std::string& key, const std::vector<uint8_t>& value);
    std::vector<uint8_t> getAuthDataBinary(const std::string& key) const;

    // Timestamps
    std::chrono::steady_clock::time_point startTime() const { return start_time_; }
    std::chrono::milliseconds elapsed() const;

    // Rate limiting info
    int failedAttempts() const { return failed_attempts_; }
    void incrementFailedAttempts();
    void resetFailedAttempts();
    std::chrono::steady_clock::time_point lockedUntil() const { return locked_until_; }
    void setLockedUntil(std::chrono::steady_clock::time_point until);
    bool isLocked() const;

    // Authenticated user info (after success)
    void setAuthenticatedUser(const std::string& user);
    const std::string& authenticatedUser() const { return authenticated_user_; }

    // Roles/groups granted
    void addRole(const std::string& role);
    const std::vector<std::string>& roles() const { return roles_; }

    // Session properties
    void setSessionProperty(const std::string& key, const std::string& value);
    std::string getSessionProperty(const std::string& key) const;

private:
    ConnectionInfo conn_info_;
    std::string username_;
    AuthType auth_type_ = AuthType::TRUST;
    AuthState state_ = AuthState::INITIAL;
    AuthFailReason fail_reason_ = AuthFailReason::NONE;
    std::string fail_message_;

    std::map<std::string, std::string> auth_data_;
    std::map<std::string, std::vector<uint8_t>> auth_data_binary_;

    std::chrono::steady_clock::time_point start_time_;
    int failed_attempts_ = 0;
    std::chrono::steady_clock::time_point locked_until_;

    std::string authenticated_user_;
    std::vector<std::string> roles_;
    std::map<std::string, std::string> session_props_;
};

// ============================================================================
// Authentication Result
// ============================================================================

/**
 * Authentication step result
 */
struct AuthResult {
    AuthState state = AuthState::INITIAL;
    AuthFailReason failure_reason = AuthFailReason::NONE;
    std::string failure_message;

    // For multi-step auth (SCRAM, etc.)
    std::vector<uint8_t> response_data;  // Data to send back to client
    bool requires_response = false;       // Client must send more data

    // For success
    std::string authenticated_user;       // May differ from requested username
    std::vector<std::string> roles;       // Granted roles

    // Warnings
    std::vector<std::string> warnings;    // e.g., "Password will expire in 7 days"

    static AuthResult success(const std::string& user = "");
    static AuthResult failure(AuthFailReason reason, const std::string& message = "");
    static AuthResult continueAuth(const std::vector<uint8_t>& data);
};

// ============================================================================
// Authentication Method Interface
// ============================================================================

/**
 * Authentication method base class
 *
 * Implement this interface for each authentication method.
 */
class AuthMethod {
public:
    virtual ~AuthMethod() = default;

    /**
     * Get method type
     */
    virtual AuthType type() const = 0;

    /**
     * Get method name
     */
    virtual const char* name() const = 0;

    /**
     * Initialize method with configuration
     */
    virtual core::Status initialize(const std::map<std::string, std::string>& config,
                                     core::ErrorContext* ctx = nullptr) = 0;

    /**
     * Start authentication
     *
     * Called at the beginning of authentication. May return:
     * - SUCCESS: Trust auth, user authenticated
     * - FAILURE: Reject auth, or validation failure
     * - IN_PROGRESS: Multi-step auth, need initial exchange
     *
     * @param ctx Authentication context
     * @return Authentication result
     */
    virtual AuthResult start(AuthContext& ctx) = 0;

    /**
     * Continue authentication
     *
     * Called for multi-step authentication with client response data.
     *
     * @param ctx Authentication context
     * @param data Client response data
     * @return Authentication result
     */
    virtual AuthResult continueAuth(AuthContext& ctx,
                                     const std::vector<uint8_t>& data) = 0;

    /**
     * Abort authentication
     *
     * Called to clean up if authentication is cancelled.
     */
    virtual void abort(AuthContext& ctx) = 0;

    /**
     * Check if method supports password verification
     * (for password change operations)
     */
    virtual bool supportsPasswordVerification() const { return false; }

    /**
     * Verify password (for methods that support it)
     */
    virtual bool verifyPassword(const std::string& username,
                                const std::string& password) { return false; }

    /**
     * Check if method is suitable for given connection
     */
    virtual bool isSuitable(const ConnectionInfo& conn) const { return true; }
};

// ============================================================================
// Built-in Authentication Methods
// ============================================================================

/**
 * Trust authentication - always succeed
 */
class TrustAuthMethod : public AuthMethod {
public:
    AuthType type() const override { return AuthType::TRUST; }
    const char* name() const override { return "trust"; }

    core::Status initialize(const std::map<std::string, std::string>& config,
                             core::ErrorContext* ctx = nullptr) override;
    AuthResult start(AuthContext& ctx) override;
    AuthResult continueAuth(AuthContext& ctx, const std::vector<uint8_t>& data) override;
    void abort(AuthContext& ctx) override;
};

/**
 * Reject authentication - always fail
 */
class RejectAuthMethod : public AuthMethod {
public:
    AuthType type() const override { return AuthType::REJECT; }
    const char* name() const override { return "reject"; }

    core::Status initialize(const std::map<std::string, std::string>& config,
                             core::ErrorContext* ctx = nullptr) override;
    AuthResult start(AuthContext& ctx) override;
    AuthResult continueAuth(AuthContext& ctx, const std::vector<uint8_t>& data) override;
    void abort(AuthContext& ctx) override;
};

/**
 * Peer authentication - Unix socket peer credentials
 */
class PeerAuthMethod : public AuthMethod {
public:
    AuthType type() const override { return AuthType::PEER; }
    const char* name() const override { return "peer"; }

    core::Status initialize(const std::map<std::string, std::string>& config,
                             core::ErrorContext* ctx = nullptr) override;
    AuthResult start(AuthContext& ctx) override;
    AuthResult continueAuth(AuthContext& ctx, const std::vector<uint8_t>& data) override;
    void abort(AuthContext& ctx) override;
    bool isSuitable(const ConnectionInfo& conn) const override;

private:
    std::map<std::string, std::string> username_map_;  // OS user -> DB user
};

// ============================================================================
// Authentication Method Factory
// ============================================================================

/**
 * Create authentication method by type
 */
std::unique_ptr<AuthMethod> createAuthMethod(AuthType type);

/**
 * Register custom authentication method
 */
using AuthMethodFactory = std::function<std::unique_ptr<AuthMethod>()>;
void registerAuthMethod(AuthType type, AuthMethodFactory factory);

}  // namespace security
}  // namespace scratchbird

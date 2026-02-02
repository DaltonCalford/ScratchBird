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
 * ScratchBird Authentication Manager
 *
 * Alpha 3 Phase 3.4: Security Suite
 *
 * Central authentication coordinator:
 * - Host-Based Authentication (HBA) rules
 * - Authentication method dispatch
 * - Rate limiting
 * - Audit logging
 * - User credential management
 */

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <regex>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/security/auth_method.h"

namespace scratchbird {
namespace security {

// ============================================================================
// Host-Based Authentication (HBA)
// ============================================================================

/**
 * HBA connection type
 */
enum class HBAConnectionType : uint8_t {
    LOCAL = 0,      // Unix domain socket
    HOST = 1,       // TCP/IP (with or without SSL)
    HOSTSSL = 2,    // TCP/IP with SSL required
    HOSTNOSSL = 3,  // TCP/IP without SSL
    HOSTGSSENC = 4  // TCP/IP with GSSAPI encryption
};

/**
 * IP address match type
 */
enum class IPMatchType : uint8_t {
    ANY = 0,        // Match any address
    SINGLE = 1,     // Single IP address
    CIDR = 2,       // CIDR notation (address/prefix)
    SAMEHOST = 3,   // Same host (127.0.0.1/::1)
    SAMENET = 4     // Same subnet
};

/**
 * HBA rule entry
 */
struct HBARule {
    // Connection type
    HBAConnectionType connection_type = HBAConnectionType::HOST;

    // Database matching
    std::string database;           // Database name, "all", "sameuser", "samerole", "@file"
    std::vector<std::string> databases;  // Expanded database list (from @file)
    bool database_is_regex = false;
    std::regex database_regex;

    // User matching
    std::string user;               // Username, "all", "+role", "@file"
    std::vector<std::string> users; // Expanded user list
    bool user_is_regex = false;
    std::regex user_regex;

    // Address matching
    IPMatchType ip_match_type = IPMatchType::ANY;
    std::string address;            // IP address or "all", "samehost", "samenet"
    std::string netmask;            // Netmask (CIDR or dotted)
    uint8_t prefix_length = 0;      // CIDR prefix length

    // Authentication
    AuthType auth_type = AuthType::REJECT;
    std::map<std::string, std::string> auth_options;

    // Rule metadata
    int line_number = 0;            // Line in pg_hba.conf
    std::string comment;            // Comment for documentation

    /**
     * Check if rule matches connection
     */
    bool matches(const ConnectionInfo& conn, const std::string& username,
                 const std::string& database, const std::vector<std::string>& roles) const;

    /**
     * Parse HBA rule from line
     */
    static core::Status parse(const std::string& line, HBARule& rule,
                               core::ErrorContext* ctx = nullptr);
};

/**
 * HBA configuration
 */
class HBAConfig {
public:
    HBAConfig();
    ~HBAConfig();

    /**
     * Load HBA configuration from file
     */
    core::Status loadFromFile(const std::string& path,
                               core::ErrorContext* ctx = nullptr);

    /**
     * Load HBA configuration from string
     */
    core::Status loadFromString(const std::string& content,
                                 core::ErrorContext* ctx = nullptr);

    /**
     * Add rule programmatically
     */
    void addRule(const HBARule& rule);

    /**
     * Clear all rules
     */
    void clear();

    /**
     * Find matching rule for connection
     */
    const HBARule* findMatchingRule(const ConnectionInfo& conn,
                                     const std::string& username,
                                     const std::string& database,
                                     const std::vector<std::string>& roles) const;

    /**
     * Get all rules
     */
    const std::vector<HBARule>& rules() const { return rules_; }

    /**
     * Get configuration file path
     */
    const std::string& configPath() const { return config_path_; }

    /**
     * Reload configuration
     */
    core::Status reload(core::ErrorContext* ctx = nullptr);

private:
    core::Status parseFile(const std::string& content, core::ErrorContext* ctx);
    void expandIncludes(const std::string& base_path);

    std::vector<HBARule> rules_;
    std::string config_path_;
    mutable std::mutex mutex_;
};

// ============================================================================
// Rate Limiting
// ============================================================================

/**
 * Rate limiter for authentication attempts
 */
class RateLimiter {
public:
    struct Config {
        uint32_t max_attempts = 5;              // Max failed attempts
        std::chrono::seconds lockout_duration{300};  // Lockout time (5 min)
        std::chrono::seconds window{60};       // Sliding window for attempts
        bool per_user = true;                  // Track per user
        bool per_address = true;               // Track per address

        static Config defaultConfig() {
            return Config{};
        }
    };

    RateLimiter();  // Uses default config
    explicit RateLimiter(const Config& config);
    ~RateLimiter();

    /**
     * Check if request should be allowed
     */
    bool allow(const std::string& username, const std::string& address);

    /**
     * Record failed attempt
     */
    void recordFailure(const std::string& username, const std::string& address);

    /**
     * Record successful authentication (reset counters)
     */
    void recordSuccess(const std::string& username, const std::string& address);

    /**
     * Check if user/address is currently locked
     */
    bool isLocked(const std::string& username, const std::string& address) const;

    /**
     * Get remaining lockout time
     */
    std::chrono::seconds remainingLockout(const std::string& username,
                                           const std::string& address) const;

    /**
     * Clear all rate limiting state
     */
    void clear();

    /**
     * Get configuration
     */
    const Config& config() const { return config_; }

private:
    struct AttemptRecord {
        std::vector<std::chrono::steady_clock::time_point> attempts;
        std::chrono::steady_clock::time_point locked_until;
    };

    std::string makeKey(const std::string& username, const std::string& address) const;
    void cleanupOldAttempts(AttemptRecord& record);

    Config config_;
    mutable std::mutex mutex_;
    std::map<std::string, AttemptRecord> records_;
};

// ============================================================================
// Audit Logging
// ============================================================================

/**
 * Authentication audit event
 */
struct AuthAuditEvent {
    enum class Type {
        AUTH_START,
        AUTH_SUCCESS,
        AUTH_FAILURE,
        PASSWORD_CHANGE,
        ACCOUNT_LOCKED,
        ACCOUNT_UNLOCKED,
        SESSION_START,
        SESSION_END
    };

    Type type;
    std::chrono::system_clock::time_point timestamp;
    std::string username;
    std::string client_address;
    uint16_t client_port = 0;
    std::string database;
    std::string protocol;
    AuthType auth_type;
    AuthFailReason failure_reason = AuthFailReason::NONE;
    std::string failure_message;
    std::string session_id;
    std::map<std::string, std::string> extra_data;
};

/**
 * Audit logger interface
 */
class AuditLogger {
public:
    virtual ~AuditLogger() = default;

    /**
     * Log authentication event
     */
    virtual void log(const AuthAuditEvent& event) = 0;

    /**
     * Flush pending logs
     */
    virtual void flush() = 0;
};

/**
 * File-based audit logger
 */
class FileAuditLogger : public AuditLogger {
public:
    explicit FileAuditLogger(const std::string& path);
    ~FileAuditLogger();

    void log(const AuthAuditEvent& event) override;
    void flush() override;

private:
    std::string path_;
    FILE* file_ = nullptr;
    std::mutex mutex_;
};

/**
 * Syslog audit logger
 */
class SyslogAuditLogger : public AuditLogger {
public:
    explicit SyslogAuditLogger(const std::string& ident = "scratchbird");
    ~SyslogAuditLogger();

    void log(const AuthAuditEvent& event) override;
    void flush() override;

private:
    std::string ident_;
};

// ============================================================================
// Credential Storage
// ============================================================================

/**
 * User credential entry
 */
struct UserCredential {
    std::string username;

    // Password authentication
    std::vector<uint8_t> password_hash;
    std::string password_algorithm;  // "scram-sha-256", "pbkdf2", etc.
    std::vector<uint8_t> password_salt;
    uint32_t password_iterations = 0;

    // SCRAM-specific
    std::vector<uint8_t> scram_stored_key;
    std::vector<uint8_t> scram_server_key;

    // Metadata
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point password_changed_at;
    std::chrono::system_clock::time_point password_expires_at;
    bool account_disabled = false;
    bool password_expired = false;

    // Roles
    std::vector<std::string> roles;

    // Connection limits
    int max_connections = 0;  // 0 = unlimited

    // Properties
    std::map<std::string, std::string> properties;
};

/**
 * Credential store interface
 */
class CredentialStore {
public:
    virtual ~CredentialStore() = default;

    /**
     * Get user credential
     */
    virtual core::Status getCredential(const std::string& username,
                                        UserCredential& cred) = 0;

    /**
     * Check if user exists
     */
    virtual bool userExists(const std::string& username) = 0;

    /**
     * Create user
     */
    virtual core::Status createUser(const UserCredential& cred) = 0;

    /**
     * Update user credential
     */
    virtual core::Status updateCredential(const UserCredential& cred) = 0;

    /**
     * Delete user
     */
    virtual core::Status deleteUser(const std::string& username) = 0;

    /**
     * List all users
     */
    virtual std::vector<std::string> listUsers() = 0;
};

// ============================================================================
// Authentication Manager
// ============================================================================

/**
 * Authentication manager configuration
 */
struct AuthManagerConfig {
    // HBA configuration
    std::string hba_file;           // pg_hba.conf path
    bool hba_enabled = true;

    // Rate limiting
    RateLimiter::Config rate_limit;
    bool rate_limit_enabled = true;

    // Audit logging
    std::string audit_log_file;
    bool audit_enabled = true;
    bool log_connections = true;
    bool log_disconnections = true;

    // Timeouts
    std::chrono::seconds auth_timeout{30};

    // Default authentication
    AuthType default_auth_type = AuthType::SCRAM_SHA_256;

    // Allow superuser remote login
    bool allow_superuser_remote = false;
};

/**
 * AuthManager - Central authentication coordinator
 */
class AuthManager {
public:
    AuthManager();
    ~AuthManager();

    // Disable copy
    AuthManager(const AuthManager&) = delete;
    AuthManager& operator=(const AuthManager&) = delete;

    /**
     * Initialize authentication manager
     */
    core::Status initialize(const AuthManagerConfig& config,
                             core::ErrorContext* ctx = nullptr);

    /**
     * Shutdown authentication manager
     */
    void shutdown();

    /**
     * Set credential store
     */
    void setCredentialStore(std::shared_ptr<CredentialStore> store);

    /**
     * Set audit logger
     */
    void setAuditLogger(std::shared_ptr<AuditLogger> logger);

    // ========================================================================
    // Authentication
    // ========================================================================

    /**
     * Start authentication for a connection
     *
     * Determines the appropriate auth method based on HBA rules
     * and initiates the authentication process.
     *
     * @param ctx Authentication context (with connection info set)
     * @return Authentication result
     */
    AuthResult startAuthentication(AuthContext& ctx);

    /**
     * Continue authentication with client response
     *
     * @param ctx Authentication context
     * @param data Client response data
     * @return Authentication result
     */
    AuthResult continueAuthentication(AuthContext& ctx,
                                       const std::vector<uint8_t>& data);

    /**
     * Abort authentication
     */
    void abortAuthentication(AuthContext& ctx);

    /**
     * Verify password (for password change)
     */
    bool verifyPassword(const std::string& username,
                        const std::string& password);

    // ========================================================================
    // User Management
    // ========================================================================

    /**
     * Create user with password
     */
    core::Status createUser(const std::string& username,
                             const std::string& password,
                             core::ErrorContext* ctx = nullptr);

    /**
     * Change user password
     */
    core::Status changePassword(const std::string& username,
                                 const std::string& old_password,
                                 const std::string& new_password,
                                 core::ErrorContext* ctx = nullptr);

    /**
     * Set user password (admin, no old password required)
     */
    core::Status setPassword(const std::string& username,
                              const std::string& new_password,
                              core::ErrorContext* ctx = nullptr);

    /**
     * Disable user account
     */
    core::Status disableUser(const std::string& username);

    /**
     * Enable user account
     */
    core::Status enableUser(const std::string& username);

    /**
     * Check if user exists
     */
    bool userExists(const std::string& username);

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * Reload HBA configuration
     */
    core::Status reloadHBA(core::ErrorContext* ctx = nullptr);

    /**
     * Get HBA configuration
     */
    const HBAConfig& hbaConfig() const { return hba_config_; }

    /**
     * Get configuration
     */
    const AuthManagerConfig& config() const { return config_; }

    // ========================================================================
    // Statistics
    // ========================================================================

    struct Stats {
        std::atomic<uint64_t> total_authentications{0};
        std::atomic<uint64_t> successful_authentications{0};
        std::atomic<uint64_t> failed_authentications{0};
        std::atomic<uint64_t> rate_limited{0};
        std::atomic<uint64_t> hba_rejected{0};
    };

    const Stats& stats() const { return stats_; }

private:
    AuthResult doAuthentication(AuthContext& ctx, const HBARule& rule);
    void logAuthEvent(AuthAuditEvent::Type type, const AuthContext& ctx,
                      const AuthResult& result = {});

    // Hash password for storage
    core::Status hashPassword(const std::string& password,
                               UserCredential& cred);

    AuthManagerConfig config_;
    HBAConfig hba_config_;
    std::unique_ptr<RateLimiter> rate_limiter_;
    std::shared_ptr<CredentialStore> credential_store_;
    std::shared_ptr<AuditLogger> audit_logger_;

    // Auth methods by type
    std::map<AuthType, std::unique_ptr<AuthMethod>> auth_methods_;

    // Active authentication contexts
    std::map<AuthContext*, std::unique_ptr<AuthMethod>> active_auths_;
    std::mutex active_auths_mutex_;

    Stats stats_;
    std::atomic<bool> initialized_{false};
};

// ============================================================================
// Password Hashing Utilities
// ============================================================================

/**
 * Hash password with SCRAM-SHA-256
 */
core::Status hashPasswordScram256(
    const std::string& password,
    std::vector<uint8_t>& salt,
    uint32_t iterations,
    std::vector<uint8_t>& stored_key,
    std::vector<uint8_t>& server_key);

/**
 * Hash password with PBKDF2
 */
core::Status hashPasswordPBKDF2(
    const std::string& password,
    const std::vector<uint8_t>& salt,
    uint32_t iterations,
    std::vector<uint8_t>& hash);

/**
 * Generate random salt
 */
void generateSalt(std::vector<uint8_t>& salt, size_t length = 16);

/**
 * Constant-time comparison
 */
bool constantTimeCompare(const std::vector<uint8_t>& a,
                          const std::vector<uint8_t>& b);

}  // namespace security
}  // namespace scratchbird

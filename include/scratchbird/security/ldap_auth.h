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
 * ScratchBird LDAP/Active Directory Authentication
 *
 * LDAP/Active Directory provider authentication method.
 *
 * Defines the provider-facing contract for LDAP and LDAPS authentication with:
 * - Simple bind authentication
 * - Search + bind authentication
 * - Group membership lookup
 * - User attribute mapping
 * - Active Directory integration
 * - Connection pooling
 */

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <mutex>
#include <chrono>
#include <functional>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/security/auth_method.h"

namespace scratchbird {
namespace security {

// ============================================================================
// LDAP Configuration
// ============================================================================

/**
 * LDAP connection mode
 */
enum class LdapConnectionMode : uint8_t {
    LDAP = 0,           // Plain LDAP (port 389)
    LDAPS = 1,          // LDAP over SSL (port 636)
    STARTTLS = 2        // LDAP with STARTTLS upgrade (port 389)
};

/**
 * LDAP authentication mode
 */
enum class LdapAuthMode : uint8_t {
    SIMPLE_BIND = 0,    // Direct bind with user DN
    SEARCH_BIND = 1     // Search for user DN, then bind
};

/**
 * LDAP scope for searches
 */
enum class LdapScope : uint8_t {
    BASE = 0,           // Search base entry only
    ONE_LEVEL = 1,      // Search one level below base
    SUBTREE = 2         // Search entire subtree
};

/**
 * LDAP server configuration
 */
struct LdapServerConfig {
    // Server connection
    std::string uri;                    // ldap://host:389 or ldaps://host:636
    std::vector<std::string> backup_uris; // Backup server URIs
    LdapConnectionMode mode = LdapConnectionMode::LDAPS;

    // Timeouts
    std::chrono::seconds connect_timeout{10};
    std::chrono::seconds operation_timeout{30};
    std::chrono::seconds network_timeout{10};

    // TLS options
    bool verify_server_cert = true;
    std::string ca_cert_file;           // CA certificate file
    std::string ca_cert_dir;            // CA certificate directory
    std::string client_cert_file;       // Client certificate (for mTLS)
    std::string client_key_file;        // Client key (for mTLS)

    // Bind credentials (for search operations)
    std::string bind_dn;                // Service account DN
    std::string bind_password;          // Service account password

    // User search configuration
    std::string user_base_dn;           // Base DN for user search
    std::string user_search_filter;     // Filter (default: (uid={username}))
    LdapScope user_search_scope = LdapScope::SUBTREE;
    std::string username_attribute;     // Attribute for username (default: uid)

    // Group search configuration
    std::string group_base_dn;          // Base DN for group search
    std::string group_search_filter;    // Filter (default: (member={user_dn}))
    LdapScope group_search_scope = LdapScope::SUBTREE;
    std::string group_attribute;        // Group name attribute (default: cn)

    // Active Directory specific
    bool is_active_directory = false;
    std::string ad_domain;              // AD domain (e.g., EXAMPLE.COM)
    bool ad_use_upn = false;            // Use UPN (user@domain) instead of DN

    // Connection pooling
    uint32_t pool_size = 5;
    std::chrono::seconds pool_timeout{60};

    // User provisioning
    bool auto_create_user = false;      // Create user on first login
    std::vector<std::string> default_roles; // Roles for auto-created users
};

/**
 * LDAP user entry
 */
struct LdapUserEntry {
    std::string dn;                     // Distinguished Name
    std::string username;               // Login username
    std::string display_name;           // Display name
    std::string email;                  // Email address
    std::vector<std::string> groups;    // Group memberships
    std::map<std::string, std::string> attributes; // Other attributes
};

/**
 * LDAP group mapping rule
 */
struct LdapGroupMapping {
    std::string ldap_group_dn;          // LDAP group DN or pattern
    std::string database_role;          // Database role to grant
    bool is_pattern = false;            // Is ldap_group_dn a regex?
};

// ============================================================================
// LDAP Connection
// ============================================================================

/**
 * LDAP connection handle (opaque)
 */
struct LdapConnection;

/**
 * LDAP connection pool
 */
class LdapConnectionPool {
public:
    LdapConnectionPool();
    ~LdapConnectionPool();

    /**
     * Initialize pool with server configuration
     */
    core::Status initialize(const LdapServerConfig& config,
                            core::ErrorContext* ctx = nullptr);

    /**
     * Get a connection from the pool
     */
    std::shared_ptr<LdapConnection> acquire();

    /**
     * Release a connection back to the pool
     */
    void release(std::shared_ptr<LdapConnection> conn);

    /**
     * Test connection to server
     */
    bool testConnection();

    /**
     * Get pool statistics
     */
    struct Stats {
        uint32_t total_connections = 0;
        uint32_t available_connections = 0;
        uint32_t in_use_connections = 0;
        uint64_t total_binds = 0;
        uint64_t failed_binds = 0;
    };
    Stats getStats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// LDAP Operations
// ============================================================================

/**
 * Bind to LDAP server
 */
core::Status ldapBind(LdapConnection* conn,
                      const std::string& dn,
                      const std::string& password,
                      core::ErrorContext* ctx = nullptr);

/**
 * Search for user by username
 */
core::Status ldapSearchUser(LdapConnection* conn,
                            const LdapServerConfig& config,
                            const std::string& username,
                            LdapUserEntry& entry,
                            core::ErrorContext* ctx = nullptr);

/**
 * Get user's group memberships
 */
core::Status ldapGetGroups(LdapConnection* conn,
                           const LdapServerConfig& config,
                           const std::string& user_dn,
                           std::vector<std::string>& groups,
                           core::ErrorContext* ctx = nullptr);

/**
 * Verify user password via bind
 */
core::Status ldapVerifyPassword(LdapConnection* conn,
                                const std::string& user_dn,
                                const std::string& password,
                                core::ErrorContext* ctx = nullptr);

// ============================================================================
// LDAP Authentication Method
// ============================================================================

/**
 * LDAP Authentication Method
 *
 * Authenticates users against LDAP/Active Directory servers.
 */
class LdapAuthMethod : public AuthMethod {
public:
    LdapAuthMethod();
    ~LdapAuthMethod();

    AuthType type() const override { return AuthType::LDAP; }
    const char* name() const override { return "ldap"; }

    core::Status initialize(const std::map<std::string, std::string>& config,
                            core::ErrorContext* ctx = nullptr) override;

    AuthResult start(AuthContext& ctx) override;
    AuthResult continueAuth(AuthContext& ctx,
                            const std::vector<uint8_t>& data) override;
    void abort(AuthContext& ctx) override;

    bool supportsPasswordVerification() const override { return true; }
    bool verifyPassword(const std::string& username,
                        const std::string& password) override;

    /**
     * Set server configuration
     */
    void setServerConfig(const LdapServerConfig& config);
    const LdapServerConfig& serverConfig() const { return config_; }

    /**
     * Add group mapping rule
     */
    void addGroupMapping(const LdapGroupMapping& mapping);

    /**
     * Look up user in LDAP
     */
    core::Status lookupUser(const std::string& username,
                            LdapUserEntry& entry,
                            core::ErrorContext* ctx = nullptr);

    /**
     * Map LDAP groups to database roles
     */
    std::vector<std::string> mapGroupsToRoles(const std::vector<std::string>& groups);

private:
    AuthResult authenticateUser(AuthContext& ctx,
                                const std::string& password);

    LdapServerConfig config_;
    std::vector<LdapGroupMapping> group_mappings_;
    std::unique_ptr<LdapConnectionPool> pool_;
    std::mutex mutex_;
};

// ============================================================================
// Active Directory Authentication
// ============================================================================

/**
 * Active Directory Authentication Method
 *
 * Specialization of LDAP auth for Active Directory with:
 * - UPN authentication (user@domain)
 * - Nested group resolution
 * - AD-specific attribute mapping
 */
class ActiveDirectoryAuthMethod : public LdapAuthMethod {
public:
    ActiveDirectoryAuthMethod();
    ~ActiveDirectoryAuthMethod();

    const char* name() const override { return "ad"; }

    core::Status initialize(const std::map<std::string, std::string>& config,
                            core::ErrorContext* ctx = nullptr) override;

    /**
     * Resolve nested group memberships (AD uses nested groups)
     */
    core::Status resolveNestedGroups(const std::string& user_dn,
                                     std::vector<std::string>& groups,
                                     core::ErrorContext* ctx = nullptr);

    /**
     * Get user's AD principal name
     */
    std::string getUserPrincipalName(const std::string& username) const;

private:
    std::string domain_;
    bool use_upn_ = true;
};

// ============================================================================
// LDAP Utility Functions
// ============================================================================

/**
 * Escape LDAP filter special characters
 */
std::string ldapEscapeFilter(const std::string& value);

/**
 * Escape LDAP DN special characters
 */
std::string ldapEscapeDN(const std::string& value);

/**
 * Build user search filter
 */
std::string buildUserSearchFilter(const std::string& filter_template,
                                  const std::string& username);

/**
 * Build group search filter
 */
std::string buildGroupSearchFilter(const std::string& filter_template,
                                   const std::string& user_dn);

/**
 * Parse LDAP URI
 */
struct LdapUri {
    std::string scheme;     // ldap or ldaps
    std::string host;
    uint16_t port = 389;
    std::string base_dn;
};
bool parseLdapUri(const std::string& uri, LdapUri& parsed);

/**
 * Convert LDAP error code to status
 */
core::Status ldapErrorToStatus(int ldap_error, const std::string& operation);

}  // namespace security
}  // namespace scratchbird

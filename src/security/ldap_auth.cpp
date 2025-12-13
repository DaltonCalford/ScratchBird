/**
 * ScratchBird LDAP/Active Directory Authentication Implementation
 *
 * Alpha 3 Phase 3.5: Security Suite - Enterprise
 */

#include "scratchbird/security/ldap_auth.h"

#include <algorithm>
#include <cstring>
#include <regex>
#include <sstream>

// Note: Full LDAP implementation would require libldap
// This provides the framework and stub implementations

namespace scratchbird {
namespace security {

// ============================================================================
// LdapConnection Implementation (Stub)
// ============================================================================

struct LdapConnection {
    void* handle = nullptr;  // Would be LDAP* from libldap
    bool connected = false;
    bool bound = false;
    std::string bound_dn;
    std::chrono::steady_clock::time_point last_used;
};

// ============================================================================
// LdapConnectionPool Implementation
// ============================================================================

struct LdapConnectionPool::Impl {
    LdapServerConfig config;
    std::vector<std::shared_ptr<LdapConnection>> available;
    std::vector<std::shared_ptr<LdapConnection>> in_use;
    std::mutex mutex;
    Stats stats;
    bool initialized = false;
};

LdapConnectionPool::LdapConnectionPool()
    : impl_(std::make_unique<Impl>())
{}

LdapConnectionPool::~LdapConnectionPool() = default;

core::Status LdapConnectionPool::initialize(const LdapServerConfig& config,
                                            core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    impl_->config = config;

    // Pre-create connections based on pool size
    for (uint32_t i = 0; i < config.pool_size; ++i) {
        auto conn = std::make_shared<LdapConnection>();
        // In real implementation, would connect to LDAP server here
        conn->connected = true;
        conn->last_used = std::chrono::steady_clock::now();
        impl_->available.push_back(conn);
        impl_->stats.total_connections++;
    }

    impl_->stats.available_connections = impl_->available.size();
    impl_->initialized = true;

    return core::Status::OK;
}

std::shared_ptr<LdapConnection> LdapConnectionPool::acquire() {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    if (impl_->available.empty()) {
        // Create new connection if under limit
        if (impl_->in_use.size() < impl_->config.pool_size * 2) {
            auto conn = std::make_shared<LdapConnection>();
            conn->connected = true;
            conn->last_used = std::chrono::steady_clock::now();
            impl_->in_use.push_back(conn);
            impl_->stats.total_connections++;
            impl_->stats.in_use_connections++;
            return conn;
        }
        return nullptr;
    }

    auto conn = impl_->available.back();
    impl_->available.pop_back();
    impl_->in_use.push_back(conn);

    impl_->stats.available_connections = impl_->available.size();
    impl_->stats.in_use_connections = impl_->in_use.size();

    return conn;
}

void LdapConnectionPool::release(std::shared_ptr<LdapConnection> conn) {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    auto it = std::find(impl_->in_use.begin(), impl_->in_use.end(), conn);
    if (it != impl_->in_use.end()) {
        impl_->in_use.erase(it);
        conn->last_used = std::chrono::steady_clock::now();
        impl_->available.push_back(conn);

        impl_->stats.available_connections = impl_->available.size();
        impl_->stats.in_use_connections = impl_->in_use.size();
    }
}

bool LdapConnectionPool::testConnection() {
    auto conn = acquire();
    if (!conn) return false;

    bool ok = conn->connected;
    release(conn);
    return ok;
}

LdapConnectionPool::Stats LdapConnectionPool::getStats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->stats;
}

// ============================================================================
// LDAP Operations (Stubs)
// ============================================================================

core::Status ldapBind(LdapConnection* conn,
                      const std::string& dn,
                      const std::string& password,
                      core::ErrorContext* ctx) {
    if (!conn || !conn->connected) {
        if (ctx) ctx->message = "LDAP connection not available";
        return core::Status::CONNECTION_FAILURE;
    }

    // Stub: In real implementation, would call ldap_simple_bind_s or ldap_sasl_bind_s
    // For now, simulate successful bind
    conn->bound = true;
    conn->bound_dn = dn;

    return core::Status::OK;
}

core::Status ldapSearchUser(LdapConnection* conn,
                            const LdapServerConfig& config,
                            const std::string& username,
                            LdapUserEntry& entry,
                            core::ErrorContext* ctx) {
    if (!conn || !conn->connected) {
        if (ctx) ctx->message = "LDAP connection not available";
        return core::Status::CONNECTION_FAILURE;
    }

    // Build search filter
    std::string filter = buildUserSearchFilter(
        config.user_search_filter.empty() ? "(uid={username})" : config.user_search_filter,
        username);

    // Stub: In real implementation, would call ldap_search_ext_s
    // For now, return a placeholder entry
    entry.username = username;
    entry.dn = "uid=" + username + "," + config.user_base_dn;
    entry.display_name = username;
    entry.email = username + "@example.com";

    return core::Status::OK;
}

core::Status ldapGetGroups(LdapConnection* conn,
                           const LdapServerConfig& config,
                           const std::string& user_dn,
                           std::vector<std::string>& groups,
                           core::ErrorContext* ctx) {
    if (!conn || !conn->connected) {
        if (ctx) ctx->message = "LDAP connection not available";
        return core::Status::CONNECTION_FAILURE;
    }

    // Build group search filter
    std::string filter = buildGroupSearchFilter(
        config.group_search_filter.empty() ? "(member={user_dn})" : config.group_search_filter,
        user_dn);

    // Stub: Would search for groups
    groups.clear();

    return core::Status::OK;
}

core::Status ldapVerifyPassword(LdapConnection* conn,
                                const std::string& user_dn,
                                const std::string& password,
                                core::ErrorContext* ctx) {
    // Attempt to bind as the user
    return ldapBind(conn, user_dn, password, ctx);
}

// ============================================================================
// LdapAuthMethod Implementation
// ============================================================================

LdapAuthMethod::LdapAuthMethod()
    : pool_(std::make_unique<LdapConnectionPool>())
{}

LdapAuthMethod::~LdapAuthMethod() = default;

core::Status LdapAuthMethod::initialize(const std::map<std::string, std::string>& config,
                                        core::ErrorContext* ctx) {
    // Parse configuration
    auto it = config.find("uri");
    if (it != config.end()) {
        config_.uri = it->second;
    }

    it = config.find("bind_dn");
    if (it != config.end()) {
        config_.bind_dn = it->second;
    }

    it = config.find("bind_password");
    if (it != config.end()) {
        config_.bind_password = it->second;
    }

    it = config.find("user_base_dn");
    if (it != config.end()) {
        config_.user_base_dn = it->second;
    }

    it = config.find("user_search_filter");
    if (it != config.end()) {
        config_.user_search_filter = it->second;
    }

    it = config.find("group_base_dn");
    if (it != config.end()) {
        config_.group_base_dn = it->second;
    }

    it = config.find("group_search_filter");
    if (it != config.end()) {
        config_.group_search_filter = it->second;
    }

    it = config.find("auto_create_user");
    if (it != config.end()) {
        config_.auto_create_user = (it->second == "true" || it->second == "1");
    }

    // Initialize connection pool
    return pool_->initialize(config_, ctx);
}

AuthResult LdapAuthMethod::start(AuthContext& ctx) {
    // LDAP auth needs password, request it
    ctx.setState(AuthState::IN_PROGRESS);

    AuthResult result;
    result.state = AuthState::IN_PROGRESS;
    result.requires_response = true;
    // Signal that we need password data
    result.response_data = {'P', 'A', 'S', 'S', 'W', 'O', 'R', 'D'};

    return result;
}

AuthResult LdapAuthMethod::continueAuth(AuthContext& ctx,
                                        const std::vector<uint8_t>& data) {
    // Data should contain password
    std::string password(data.begin(), data.end());

    return authenticateUser(ctx, password);
}

void LdapAuthMethod::abort(AuthContext& ctx) {
    ctx.setState(AuthState::FAILURE);
    ctx.clearAuthData();
}

bool LdapAuthMethod::verifyPassword(const std::string& username,
                                    const std::string& password) {
    LdapUserEntry entry;
    core::ErrorContext ctx;

    auto status = lookupUser(username, entry, &ctx);
    if (status != core::Status::OK) {
        return false;
    }

    auto conn = pool_->acquire();
    if (!conn) {
        return false;
    }

    status = ldapVerifyPassword(conn.get(), entry.dn, password, &ctx);
    pool_->release(conn);

    return status == core::Status::OK;
}

void LdapAuthMethod::setServerConfig(const LdapServerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

void LdapAuthMethod::addGroupMapping(const LdapGroupMapping& mapping) {
    std::lock_guard<std::mutex> lock(mutex_);
    group_mappings_.push_back(mapping);
}

core::Status LdapAuthMethod::lookupUser(const std::string& username,
                                        LdapUserEntry& entry,
                                        core::ErrorContext* ctx) {
    auto conn = pool_->acquire();
    if (!conn) {
        if (ctx) ctx->message = "Failed to acquire LDAP connection";
        return core::Status::CONNECTION_FAILURE;
    }

    // Bind with service account if configured
    if (!config_.bind_dn.empty()) {
        auto status = ldapBind(conn.get(), config_.bind_dn, config_.bind_password, ctx);
        if (status != core::Status::OK) {
            pool_->release(conn);
            return status;
        }
    }

    auto status = ldapSearchUser(conn.get(), config_, username, entry, ctx);
    pool_->release(conn);

    return status;
}

std::vector<std::string> LdapAuthMethod::mapGroupsToRoles(const std::vector<std::string>& groups) {
    std::vector<std::string> roles;

    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& group : groups) {
        for (const auto& mapping : group_mappings_) {
            bool matches = false;

            if (mapping.is_pattern) {
                std::regex re(mapping.ldap_group_dn);
                matches = std::regex_match(group, re);
            } else {
                matches = (group == mapping.ldap_group_dn);
            }

            if (matches) {
                roles.push_back(mapping.database_role);
            }
        }
    }

    return roles;
}

AuthResult LdapAuthMethod::authenticateUser(AuthContext& ctx,
                                            const std::string& password) {
    LdapUserEntry entry;
    core::ErrorContext err_ctx;

    // Look up user
    auto status = lookupUser(ctx.username(), entry, &err_ctx);
    if (status != core::Status::OK) {
        ctx.setFailure(AuthFailReason::USER_NOT_FOUND, "User not found in LDAP");
        return AuthResult::failure(AuthFailReason::USER_NOT_FOUND, "User not found in LDAP");
    }

    // Verify password by binding as user
    auto conn = pool_->acquire();
    if (!conn) {
        ctx.setFailure(AuthFailReason::INTERNAL_ERROR, "LDAP connection unavailable");
        return AuthResult::failure(AuthFailReason::INTERNAL_ERROR, "LDAP connection unavailable");
    }

    status = ldapVerifyPassword(conn.get(), entry.dn, password, &err_ctx);

    if (status == core::Status::OK) {
        // Get user's groups
        std::vector<std::string> groups;
        ldapGetGroups(conn.get(), config_, entry.dn, groups, nullptr);
        pool_->release(conn);

        // Map groups to roles
        auto roles = mapGroupsToRoles(groups);

        ctx.setState(AuthState::SUCCESS);
        ctx.setAuthenticatedUser(entry.username);
        for (const auto& role : roles) {
            ctx.addRole(role);
        }

        AuthResult result = AuthResult::success(entry.username);
        result.roles = roles;
        return result;
    }

    pool_->release(conn);

    ctx.setFailure(AuthFailReason::INVALID_CREDENTIALS, "LDAP bind failed");
    return AuthResult::failure(AuthFailReason::INVALID_CREDENTIALS, "Invalid password");
}

// ============================================================================
// ActiveDirectoryAuthMethod Implementation
// ============================================================================

ActiveDirectoryAuthMethod::ActiveDirectoryAuthMethod() {
    // Set AD-specific defaults
    LdapServerConfig config;
    config.is_active_directory = true;
    config.ad_use_upn = true;
    config.user_search_filter = "(sAMAccountName={username})";
    config.group_search_filter = "(member:1.2.840.113556.1.4.1941:={user_dn})"; // Recursive
    config.username_attribute = "sAMAccountName";
    setServerConfig(config);
}

ActiveDirectoryAuthMethod::~ActiveDirectoryAuthMethod() = default;

core::Status ActiveDirectoryAuthMethod::initialize(const std::map<std::string, std::string>& config,
                                                   core::ErrorContext* ctx) {
    // Get AD domain
    auto it = config.find("domain");
    if (it != config.end()) {
        domain_ = it->second;
    }

    it = config.find("use_upn");
    if (it != config.end()) {
        use_upn_ = (it->second == "true" || it->second == "1");
    }

    // Call parent initialization
    return LdapAuthMethod::initialize(config, ctx);
}

core::Status ActiveDirectoryAuthMethod::resolveNestedGroups(const std::string& user_dn,
                                                            std::vector<std::string>& groups,
                                                            core::ErrorContext* ctx) {
    // AD supports recursive group membership via LDAP_MATCHING_RULE_IN_CHAIN
    // The filter (member:1.2.840.113556.1.4.1941:={user_dn}) handles this
    auto& config = const_cast<LdapServerConfig&>(serverConfig());
    config.group_search_filter = "(member:1.2.840.113556.1.4.1941:={user_dn})";

    // Use parent's group lookup which will use the recursive filter
    // This is handled in ldapGetGroups
    return core::Status::OK;
}

std::string ActiveDirectoryAuthMethod::getUserPrincipalName(const std::string& username) const {
    if (use_upn_ && !domain_.empty()) {
        // Check if already in UPN format
        if (username.find('@') != std::string::npos) {
            return username;
        }
        return username + "@" + domain_;
    }
    return username;
}

// ============================================================================
// LDAP Utility Functions
// ============================================================================

std::string ldapEscapeFilter(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() * 2);

    for (char c : value) {
        switch (c) {
            case '\\': escaped += "\\5c"; break;
            case '*':  escaped += "\\2a"; break;
            case '(':  escaped += "\\28"; break;
            case ')':  escaped += "\\29"; break;
            case '\0': escaped += "\\00"; break;
            default:   escaped += c; break;
        }
    }

    return escaped;
}

std::string ldapEscapeDN(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() * 2);

    bool first = true;
    for (char c : value) {
        // Escape special characters in DN values
        switch (c) {
            case ',':
            case '+':
            case '"':
            case '\\':
            case '<':
            case '>':
            case ';':
                escaped += '\\';
                escaped += c;
                break;
            case ' ':
                // Escape leading/trailing spaces
                if (first || &c == &value.back()) {
                    escaped += "\\ ";
                } else {
                    escaped += c;
                }
                break;
            case '#':
                // Escape leading #
                if (first) {
                    escaped += "\\#";
                } else {
                    escaped += c;
                }
                break;
            default:
                escaped += c;
                break;
        }
        first = false;
    }

    return escaped;
}

std::string buildUserSearchFilter(const std::string& filter_template,
                                  const std::string& username) {
    std::string filter = filter_template;
    std::string escaped_username = ldapEscapeFilter(username);

    // Replace {username} placeholder
    size_t pos = filter.find("{username}");
    while (pos != std::string::npos) {
        filter.replace(pos, 10, escaped_username);
        pos = filter.find("{username}", pos + escaped_username.length());
    }

    return filter;
}

std::string buildGroupSearchFilter(const std::string& filter_template,
                                   const std::string& user_dn) {
    std::string filter = filter_template;
    std::string escaped_dn = ldapEscapeFilter(user_dn);

    // Replace {user_dn} placeholder
    size_t pos = filter.find("{user_dn}");
    while (pos != std::string::npos) {
        filter.replace(pos, 9, escaped_dn);
        pos = filter.find("{user_dn}", pos + escaped_dn.length());
    }

    return filter;
}

bool parseLdapUri(const std::string& uri, LdapUri& parsed) {
    // Parse ldap://host:port/base_dn or ldaps://host:port/base_dn
    std::regex uri_regex(R"((ldaps?)://([^:/]+)(?::(\d+))?(?:/(.*))?)", std::regex::icase);
    std::smatch match;

    if (!std::regex_match(uri, match, uri_regex)) {
        return false;
    }

    parsed.scheme = match[1].str();
    std::transform(parsed.scheme.begin(), parsed.scheme.end(),
                   parsed.scheme.begin(), ::tolower);

    parsed.host = match[2].str();

    if (match[3].matched) {
        parsed.port = static_cast<uint16_t>(std::stoi(match[3].str()));
    } else {
        parsed.port = (parsed.scheme == "ldaps") ? 636 : 389;
    }

    if (match[4].matched) {
        parsed.base_dn = match[4].str();
    }

    return true;
}

core::Status ldapErrorToStatus(int ldap_error, const std::string& operation) {
    // Map LDAP error codes to Status
    // These are standard LDAP result codes
    switch (ldap_error) {
        case 0:  // LDAP_SUCCESS
            return core::Status::OK;
        case 1:  // LDAP_OPERATIONS_ERROR
            return core::Status::INTERNAL_ERROR;
        case 2:  // LDAP_PROTOCOL_ERROR
            return core::Status::PROTOCOL_VIOLATION;
        case 3:  // LDAP_TIMELIMIT_EXCEEDED
            return core::Status::LOCK_TIMEOUT;
        case 4:  // LDAP_SIZELIMIT_EXCEEDED
            return core::Status::OOM;
        case 7:  // LDAP_AUTH_METHOD_NOT_SUPPORTED
            return core::Status::NOT_SUPPORTED;
        case 8:  // LDAP_STRONG_AUTH_REQUIRED
            return core::Status::PERMISSION_DENIED;
        case 32: // LDAP_NO_SUCH_OBJECT
            return core::Status::NOT_FOUND;
        case 49: // LDAP_INVALID_CREDENTIALS
            return core::Status::PERMISSION_DENIED;
        case 50: // LDAP_INSUFFICIENT_ACCESS
            return core::Status::PERMISSION_DENIED;
        case 52: // LDAP_UNAVAILABLE
            return core::Status::CONNECTION_FAILURE;
        case 53: // LDAP_UNWILLING_TO_PERFORM
            return core::Status::NOT_SUPPORTED;
        case 81: // LDAP_SERVER_DOWN
            return core::Status::CONNECTION_FAILURE;
        case 85: // LDAP_TIMEOUT
            return core::Status::LOCK_TIMEOUT;
        default:
            return core::Status::INTERNAL_ERROR;
    }
}

}  // namespace security
}  // namespace scratchbird

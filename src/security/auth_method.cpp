/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * ScratchBird Authentication Method Implementation
 *
 * Built-in and plugin-backed authentication method base implementation.
 */

#include "scratchbird/security/auth_method.h"
#include "scratchbird/security/tls_config.h"

#include <cstring>
#include <map>
#include <mutex>
#if !defined(_WIN32)
    #include <pwd.h>
    #include <grp.h>
    #include <unistd.h>
#endif

namespace scratchbird {
namespace security {

// ============================================================================
// Auth Type String Conversion
// ============================================================================

const char* authTypeToString(AuthType type) {
    switch (type) {
        case AuthType::TRUST: return "trust";
        case AuthType::REJECT: return "reject";
        case AuthType::PASSWORD: return "password";
        case AuthType::MD5: return "md5";
        case AuthType::SCRAM_SHA_256: return "scram-sha-256";
        case AuthType::SCRAM_SHA_512: return "scram-sha-512";
        case AuthType::CERTIFICATE: return "cert";
        case AuthType::LDAP: return "ldap";
        case AuthType::KERBEROS: return "gss";
        case AuthType::PEER: return "peer";
        case AuthType::IDENT: return "ident";
        case AuthType::RADIUS: return "radius";
        case AuthType::PAM: return "pam";
        case AuthType::TOKEN: return "token";
        default: return "unknown";
    }
}

bool parseAuthType(const std::string& str, AuthType& type) {
    static const std::map<std::string, AuthType> type_map = {
        {"trust", AuthType::TRUST},
        {"reject", AuthType::REJECT},
        {"password", AuthType::PASSWORD},
        {"md5", AuthType::MD5},
        {"scram-sha-256", AuthType::SCRAM_SHA_256},
        {"scram-sha-512", AuthType::SCRAM_SHA_512},
        {"cert", AuthType::CERTIFICATE},
        {"clientcert", AuthType::CERTIFICATE},
        {"ldap", AuthType::LDAP},
        {"gss", AuthType::KERBEROS},
        {"gssapi", AuthType::KERBEROS},
        {"kerberos", AuthType::KERBEROS},
        {"peer", AuthType::PEER},
        {"ident", AuthType::IDENT},
        {"radius", AuthType::RADIUS},
        {"pam", AuthType::PAM},
        {"token", AuthType::TOKEN},
        {"oauth", AuthType::TOKEN},
        {"oidc", AuthType::TOKEN}
    };

    auto it = type_map.find(str);
    if (it != type_map.end()) {
        type = it->second;
        return true;
    }
    return false;
}

const char* authFailReasonToString(AuthFailReason reason) {
    switch (reason) {
        case AuthFailReason::NONE: return "none";
        case AuthFailReason::INVALID_CREDENTIALS: return "invalid_credentials";
        case AuthFailReason::USER_NOT_FOUND: return "user_not_found";
        case AuthFailReason::ACCOUNT_DISABLED: return "account_disabled";
        case AuthFailReason::ACCOUNT_LOCKED: return "account_locked";
        case AuthFailReason::PASSWORD_EXPIRED: return "password_expired";
        case AuthFailReason::CERTIFICATE_INVALID: return "certificate_invalid";
        case AuthFailReason::CERTIFICATE_EXPIRED: return "certificate_expired";
        case AuthFailReason::CERTIFICATE_REVOKED: return "certificate_revoked";
        case AuthFailReason::CERTIFICATE_NOT_TRUSTED: return "certificate_not_trusted";
        case AuthFailReason::PRINCIPAL_MISMATCH: return "principal_mismatch";
        case AuthFailReason::PROTOCOL_ERROR: return "protocol_error";
        case AuthFailReason::TIMEOUT: return "timeout";
        case AuthFailReason::RATE_LIMITED: return "rate_limited";
        case AuthFailReason::NOT_ALLOWED: return "not_allowed";
        case AuthFailReason::INTERNAL_ERROR: return "internal_error";
        default: return "unknown";
    }
}

// ============================================================================
// AuthContext Implementation
// ============================================================================

AuthContext::AuthContext()
    : start_time_(std::chrono::steady_clock::now())
{}

AuthContext::~AuthContext() = default;

void AuthContext::setConnectionInfo(const ConnectionInfo& info) {
    conn_info_ = info;
}

void AuthContext::setUsername(const std::string& username) {
    username_ = username;
}

void AuthContext::setAuthType(AuthType type) {
    auth_type_ = type;
}

void AuthContext::setState(AuthState state) {
    state_ = state;
}

void AuthContext::setFailure(AuthFailReason reason, const std::string& message) {
    state_ = AuthState::FAILURE;
    fail_reason_ = reason;
    fail_message_ = message;
}

void AuthContext::setAuthData(const std::string& key, const std::string& value) {
    auth_data_[key] = value;
}

std::string AuthContext::getAuthData(const std::string& key) const {
    auto it = auth_data_.find(key);
    return (it != auth_data_.end()) ? it->second : "";
}

bool AuthContext::hasAuthData(const std::string& key) const {
    return auth_data_.find(key) != auth_data_.end();
}

void AuthContext::clearAuthData() {
    auth_data_.clear();
    auth_data_binary_.clear();
}

void AuthContext::setAuthDataBinary(const std::string& key, const std::vector<uint8_t>& value) {
    auth_data_binary_[key] = value;
}

std::vector<uint8_t> AuthContext::getAuthDataBinary(const std::string& key) const {
    auto it = auth_data_binary_.find(key);
    return (it != auth_data_binary_.end()) ? it->second : std::vector<uint8_t>{};
}

std::chrono::milliseconds AuthContext::elapsed() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_);
}

void AuthContext::incrementFailedAttempts() {
    failed_attempts_++;
}

void AuthContext::resetFailedAttempts() {
    failed_attempts_ = 0;
}

void AuthContext::setLockedUntil(std::chrono::steady_clock::time_point until) {
    locked_until_ = until;
}

bool AuthContext::isLocked() const {
    return locked_until_ > std::chrono::steady_clock::now();
}

void AuthContext::setAuthenticatedUser(const std::string& user) {
    authenticated_user_ = user;
    state_ = AuthState::SUCCESS;
}

void AuthContext::addRole(const std::string& role) {
    roles_.push_back(role);
}

void AuthContext::setSessionProperty(const std::string& key, const std::string& value) {
    session_props_[key] = value;
}

std::string AuthContext::getSessionProperty(const std::string& key) const {
    auto it = session_props_.find(key);
    return (it != session_props_.end()) ? it->second : "";
}

// ============================================================================
// AuthResult Implementation
// ============================================================================

AuthResult AuthResult::success(const std::string& user) {
    AuthResult result;
    result.state = AuthState::SUCCESS;
    result.authenticated_user = user;
    return result;
}

AuthResult AuthResult::failure(AuthFailReason reason, const std::string& message) {
    AuthResult result;
    result.state = AuthState::FAILURE;
    result.failure_reason = reason;
    result.failure_message = message;
    return result;
}

AuthResult AuthResult::continueAuth(const std::vector<uint8_t>& data) {
    AuthResult result;
    result.state = AuthState::IN_PROGRESS;
    result.response_data = data;
    result.requires_response = true;
    return result;
}

// ============================================================================
// TrustAuthMethod Implementation
// ============================================================================

core::Status TrustAuthMethod::initialize(
    const std::map<std::string, std::string>& /*config*/,
    core::ErrorContext* /*ctx*/)
{
    return core::Status::OK;
}

AuthResult TrustAuthMethod::start(AuthContext& ctx) {
    // Trust authentication always succeeds immediately
    ctx.setAuthenticatedUser(ctx.username());
    return AuthResult::success(ctx.username());
}

AuthResult TrustAuthMethod::continueAuth(
    AuthContext& /*ctx*/,
    const std::vector<uint8_t>& /*data*/)
{
    // Trust doesn't have multi-step auth
    return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR,
                               "Trust authentication does not require response");
}

void TrustAuthMethod::abort(AuthContext& /*ctx*/) {
    // Nothing to clean up
}

// ============================================================================
// RejectAuthMethod Implementation
// ============================================================================

core::Status RejectAuthMethod::initialize(
    const std::map<std::string, std::string>& /*config*/,
    core::ErrorContext* /*ctx*/)
{
    return core::Status::OK;
}

AuthResult RejectAuthMethod::start(AuthContext& ctx) {
    ctx.setFailure(AuthFailReason::NOT_ALLOWED, "Authentication rejected by server");
    return AuthResult::failure(AuthFailReason::NOT_ALLOWED,
                               "Authentication rejected by server");
}

AuthResult RejectAuthMethod::continueAuth(
    AuthContext& /*ctx*/,
    const std::vector<uint8_t>& /*data*/)
{
    return AuthResult::failure(AuthFailReason::NOT_ALLOWED,
                               "Authentication rejected by server");
}

void RejectAuthMethod::abort(AuthContext& /*ctx*/) {
    // Nothing to clean up
}

// ============================================================================
// PeerAuthMethod Implementation
// ============================================================================

core::Status PeerAuthMethod::initialize(
    const std::map<std::string, std::string>& config,
    core::ErrorContext* /*ctx*/)
{
    // Load username mapping if specified
    auto it = config.find("map");
    if (it != config.end()) {
        // Parse map file or inline map
        // Format: "osuser1=dbuser1,osuser2=dbuser2"
        // For simplicity, inline parsing
        std::string map_str = it->second;
        size_t pos = 0;
        while (pos < map_str.size()) {
            size_t eq_pos = map_str.find('=', pos);
            if (eq_pos == std::string::npos) break;

            size_t comma_pos = map_str.find(',', eq_pos);
            if (comma_pos == std::string::npos) comma_pos = map_str.size();

            std::string os_user = map_str.substr(pos, eq_pos - pos);
            std::string db_user = map_str.substr(eq_pos + 1, comma_pos - eq_pos - 1);

            username_map_[os_user] = db_user;
            pos = comma_pos + 1;
        }
    }

    return core::Status::OK;
}

AuthResult PeerAuthMethod::start(AuthContext& ctx) {
    const auto& conn = ctx.connectionInfo();

    // Peer auth only works on Unix sockets
    if (!conn.is_unix_socket) {
        return AuthResult::failure(AuthFailReason::NOT_ALLOWED,
                                   "Peer authentication only available on local connections");
    }

    // Get OS username from peer credentials
    std::string os_username = conn.peer_username;
    if (os_username.empty()) {
#if !defined(_WIN32)
        // Try to look up from UID
        struct passwd* pw = getpwuid(static_cast<uid_t>(conn.peer_uid));
        if (pw) {
            os_username = pw->pw_name;
        }
#endif
    }

    if (os_username.empty()) {
        return AuthResult::failure(AuthFailReason::INTERNAL_ERROR,
                                   "Could not determine peer username");
    }

    // Map OS username to database username
    std::string db_username = os_username;
    auto map_it = username_map_.find(os_username);
    if (map_it != username_map_.end()) {
        db_username = map_it->second;
    }

    // Check if the requested username matches
    if (ctx.username() != db_username) {
        return AuthResult::failure(AuthFailReason::PRINCIPAL_MISMATCH,
                                   "OS user '" + os_username + "' does not match database user '" +
                                   ctx.username() + "'");
    }

    ctx.setAuthenticatedUser(db_username);
    return AuthResult::success(db_username);
}

AuthResult PeerAuthMethod::continueAuth(
    AuthContext& /*ctx*/,
    const std::vector<uint8_t>& /*data*/)
{
    return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR,
                               "Peer authentication does not require response");
}

void PeerAuthMethod::abort(AuthContext& /*ctx*/) {
    // Nothing to clean up
}

bool PeerAuthMethod::isSuitable(const ConnectionInfo& conn) const {
    return conn.is_unix_socket;
}

// ============================================================================
// Auth Method Factory
// ============================================================================

static std::mutex factory_mutex;
static std::map<AuthType, AuthMethodFactory> custom_factories;

std::unique_ptr<AuthMethod> createAuthMethod(AuthType type) {
    // Check custom factories first
    {
        std::lock_guard<std::mutex> lock(factory_mutex);
        auto it = custom_factories.find(type);
        if (it != custom_factories.end()) {
            return it->second();
        }
    }

    // Built-in methods
    switch (type) {
        case AuthType::TRUST:
            return std::make_unique<TrustAuthMethod>();
        case AuthType::REJECT:
            return std::make_unique<RejectAuthMethod>();
        case AuthType::PEER:
            return std::make_unique<PeerAuthMethod>();

        // Other methods will be implemented in separate files
        case AuthType::SCRAM_SHA_256:
        case AuthType::SCRAM_SHA_512:
        case AuthType::CERTIFICATE:
        case AuthType::PASSWORD:
        case AuthType::MD5:
        case AuthType::LDAP:
        case AuthType::KERBEROS:
        case AuthType::IDENT:
        case AuthType::RADIUS:
        case AuthType::PAM:
        case AuthType::TOKEN:
            // Not yet implemented, return nullptr
            return nullptr;

        default:
            return nullptr;
    }
}

void registerAuthMethod(AuthType type, AuthMethodFactory factory) {
    std::lock_guard<std::mutex> lock(factory_mutex);
    custom_factories[type] = std::move(factory);
}

}  // namespace security
}  // namespace scratchbird

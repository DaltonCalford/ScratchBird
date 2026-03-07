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
 * ScratchBird Kerberos/GSSAPI Authentication Implementation
 *
 * Kerberos/GSSAPI provider authentication implementation.
 */

#include "scratchbird/security/kerberos_auth.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

// Note: full provider-backed GSSAPI support requires libgssapi_krb5. The
// current file provides the admission/runtime framework and stubbed behavior.

namespace scratchbird {
namespace security {

// ============================================================================
// Internal Types
// ============================================================================

struct KerberosAuthMethod::GssCredential {
    // Would hold gss_cred_id_t in real implementation
    void* handle = nullptr;
    std::string principal;
    bool valid = false;
};

struct KerberosAuthMethod::ContextState {
    // Would hold gss_ctx_id_t in real implementation
    void* context_handle = nullptr;
    GssContextState state = GssContextState::INITIAL;
    KerberosPrincipal client_principal;
    bool mutual_auth_complete = false;
    std::vector<uint8_t> delegated_creds;
};

// ============================================================================
// KerberosAuthMethod Implementation
// ============================================================================

KerberosAuthMethod::KerberosAuthMethod()
    : server_cred_(std::make_unique<GssCredential>())
{}

KerberosAuthMethod::~KerberosAuthMethod() {
    // Clean up any active contexts
    std::lock_guard<std::mutex> lock(contexts_mutex_);
    contexts_.clear();
}

core::Status KerberosAuthMethod::initialize(const std::map<std::string, std::string>& config,
                                            core::ErrorContext* ctx) {
    // Parse configuration
    auto it = config.find("keytab");
    if (it != config.end()) {
        config_.keytab_file = it->second;
    }

    it = config.find("service_name");
    if (it != config.end()) {
        config_.service_name = it->second;
    } else {
        config_.service_name = "scratchbird";
    }

    it = config.find("service_hostname");
    if (it != config.end()) {
        config_.service_hostname = it->second;
    }

    it = config.find("realm");
    if (it != config.end()) {
        config_.realm = it->second;
    } else {
        config_.realm = getDefaultRealm();
    }

    it = config.find("include_realm");
    if (it != config.end()) {
        config_.include_realm_in_username = (it->second == "true" || it->second == "1");
    }

    it = config.find("allow_delegation");
    if (it != config.end()) {
        config_.allow_delegation = (it->second == "true" || it->second == "1");
    }

    // Validate keytab if specified
    if (!config_.keytab_file.empty()) {
        auto status = validateKeytab(config_.keytab_file, "", ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    // Initialize server credentials
    return initializeServerCredentials();
}

AuthResult KerberosAuthMethod::start(AuthContext& ctx) {
    // Initialize context state for this session
    {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        contexts_[&ctx] = std::make_unique<ContextState>();
    }

    ctx.setState(AuthState::IN_PROGRESS);

    // Return empty response to signal GSSAPI auth start
    // Client should send initial GSSAPI token
    AuthResult result;
    result.state = AuthState::IN_PROGRESS;
    result.requires_response = true;
    result.response_data.clear();  // Empty = "send your token"

    return result;
}

AuthResult KerberosAuthMethod::continueAuth(AuthContext& ctx,
                                            const std::vector<uint8_t>& data) {
    return processGssToken(ctx, data);
}

void KerberosAuthMethod::abort(AuthContext& ctx) {
    cleanupContext(ctx);
    ctx.setState(AuthState::FAILURE);
}

bool KerberosAuthMethod::isSuitable(const ConnectionInfo& conn) const {
    // Kerberos is suitable for any connection type, but especially useful for:
    // - Corporate networks with Kerberos infrastructure
    // - Single sign-on scenarios
    return true;
}

void KerberosAuthMethod::setConfig(const KerberosConfig& config) {
    config_ = config;
}

std::string KerberosAuthMethod::mapPrincipalToUser(const KerberosPrincipal& principal) {
    // Check explicit mapping first
    auto it = config_.principal_map.find(principal.full_principal);
    if (it != config_.principal_map.end()) {
        return it->second;
    }

    // Check without realm
    std::string principal_no_realm = principal.primary;
    if (!principal.instance.empty()) {
        principal_no_realm += "/" + principal.instance;
    }

    it = config_.principal_map.find(principal_no_realm);
    if (it != config_.principal_map.end()) {
        return it->second;
    }

    // Default: use primary component
    if (config_.include_realm_in_username) {
        return principal.full_principal;
    }

    return principal.primary;
}

void KerberosAuthMethod::addPrincipalMapping(const std::string& principal,
                                              const std::string& username) {
    config_.principal_map[principal] = username;
}

std::string KerberosAuthMethod::getServicePrincipal() const {
    std::string principal = config_.service_name;

    if (!config_.service_hostname.empty()) {
        principal += "/" + config_.service_hostname;
    }

    if (!config_.realm.empty()) {
        principal += "@" + config_.realm;
    }

    return principal;
}

AuthResult KerberosAuthMethod::processGssToken(AuthContext& ctx,
                                               const std::vector<uint8_t>& token) {
    std::lock_guard<std::mutex> lock(contexts_mutex_);

    auto it = contexts_.find(&ctx);
    if (it == contexts_.end()) {
        ctx.setFailure(AuthFailReason::INTERNAL_ERROR, "No GSSAPI context");
        return AuthResult::failure(AuthFailReason::INTERNAL_ERROR, "No GSSAPI context");
    }

    ContextState* state = it->second.get();

    // Stub implementation - in real code would call gss_accept_sec_context
    // For now, simulate successful authentication if token is non-empty

    if (token.empty()) {
        ctx.setFailure(AuthFailReason::PROTOCOL_ERROR, "Empty GSSAPI token");
        return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR, "Empty GSSAPI token");
    }

    // Check for SPNEGO wrapper
    bool is_spnego = SpnegoAuthMethod::isSpnegoToken(token);
    std::vector<uint8_t> inner_token = is_spnego ?
        SpnegoAuthMethod::unwrapSpnegoToken(token) : token;

    if (inner_token.empty()) {
        ctx.setFailure(AuthFailReason::PROTOCOL_ERROR, "Invalid GSSAPI token");
        return AuthResult::failure(AuthFailReason::PROTOCOL_ERROR, "Invalid GSSAPI token");
    }

    // In real implementation:
    // 1. Call gss_accept_sec_context with the token
    // 2. Check if context is complete or needs more exchanges
    // 3. Extract client principal from context
    // 4. Handle delegation if requested

    // Simulate successful authentication
    // Parse the username from the token (stub - would get from GSS context)
    std::string username = ctx.username();
    if (username.empty()) {
        // Try to extract from connection info or default
        username = "kerberos_user";
    }

    state->state = GssContextState::CONTEXT_ESTABLISHED;
    state->client_principal.full_principal = username + "@" + config_.realm;
    state->client_principal.primary = username;
    state->client_principal.realm = config_.realm;

    // Map principal to database user
    std::string db_user = mapPrincipalToUser(state->client_principal);

    ctx.setState(AuthState::SUCCESS);
    ctx.setAuthenticatedUser(db_user);

    AuthResult result = AuthResult::success(db_user);

    // If client requested mutual auth, send response token
    // Stub: in real implementation would include gss_accept output token
    if (is_spnego) {
        result.response_data = SpnegoAuthMethod::wrapSpnegoToken({}, false);
    }

    return result;
}

core::Status KerberosAuthMethod::initializeServerCredentials() {
    // In real implementation:
    // 1. Set KRB5_KTNAME environment if keytab specified
    // 2. Call gss_acquire_cred with GSS_C_ACCEPT
    // 3. Store credential handle

    if (!config_.keytab_file.empty()) {
        // Validate keytab exists and is readable
        std::ifstream file(config_.keytab_file);
        if (!file.good()) {
            return core::Status::NOT_FOUND;
        }
    }

    server_cred_->principal = getServicePrincipal();
    server_cred_->valid = true;

    return core::Status::OK;
}

void KerberosAuthMethod::cleanupContext(AuthContext& ctx) {
    std::lock_guard<std::mutex> lock(contexts_mutex_);

    auto it = contexts_.find(&ctx);
    if (it != contexts_.end()) {
        // In real implementation: call gss_delete_sec_context
        contexts_.erase(it);
    }
}

// ============================================================================
// SpnegoAuthMethod Implementation
// ============================================================================

SpnegoAuthMethod::SpnegoAuthMethod() = default;
SpnegoAuthMethod::~SpnegoAuthMethod() = default;

AuthResult SpnegoAuthMethod::start(AuthContext& ctx) {
    expect_spnego_ = true;
    return KerberosAuthMethod::start(ctx);
}

AuthResult SpnegoAuthMethod::continueAuth(AuthContext& ctx,
                                          const std::vector<uint8_t>& data) {
    // SPNEGO tokens are wrapped Kerberos tokens
    // The base class handles unwrapping
    return KerberosAuthMethod::continueAuth(ctx, data);
}

bool SpnegoAuthMethod::isSpnegoToken(const std::vector<uint8_t>& token) {
    // SPNEGO tokens start with ASN.1 SEQUENCE tag followed by SPNEGO OID
    // OID: 1.3.6.1.5.5.2 = 06 06 2b 06 01 05 05 02
    if (token.size() < 10) return false;

    // Check for APPLICATION [0] or SEQUENCE tag
    if (token[0] != 0x60 && token[0] != 0x30) return false;

    // Look for SPNEGO OID
    static const uint8_t spnego_oid[] = {0x06, 0x06, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x02};

    // Search within first 20 bytes for OID
    size_t search_len = std::min(token.size(), size_t(20));
    for (size_t i = 0; i + sizeof(spnego_oid) <= search_len; ++i) {
        if (std::memcmp(token.data() + i, spnego_oid, sizeof(spnego_oid)) == 0) {
            return true;
        }
    }

    return false;
}

std::vector<uint8_t> SpnegoAuthMethod::unwrapSpnegoToken(const std::vector<uint8_t>& token) {
    // Stub: In real implementation, would parse ASN.1 SPNEGO structure
    // and extract the mechToken field

    if (!isSpnegoToken(token)) {
        return token;  // Not SPNEGO, return as-is
    }

    // For now, skip SPNEGO wrapper heuristically
    // Real implementation would use proper ASN.1 parsing

    // Find Kerberos OID (1.2.840.113554.1.2.2)
    // or just return everything after SPNEGO header
    if (token.size() > 20) {
        // Skip approximate SPNEGO header
        return std::vector<uint8_t>(token.begin() + 15, token.end());
    }

    return {};
}

std::vector<uint8_t> SpnegoAuthMethod::wrapSpnegoToken(const std::vector<uint8_t>& token,
                                                       bool is_initial) {
    // Stub: Build SPNEGO NegTokenResp structure
    // In real implementation, would construct proper ASN.1

    if (token.empty()) {
        // Return minimal SPNEGO accept response
        return {0xa1, 0x03, 0x30, 0x01, 0x00};  // NegTokenResp with accept-complete
    }

    // Wrap token in SPNEGO structure
    std::vector<uint8_t> wrapped;
    wrapped.reserve(token.size() + 20);

    // Simplified SPNEGO wrapper (not fully spec-compliant)
    wrapped.push_back(0xa1);  // NegTokenResp
    wrapped.push_back(static_cast<uint8_t>(token.size() + 10));
    wrapped.push_back(0x30);  // SEQUENCE
    wrapped.push_back(static_cast<uint8_t>(token.size() + 8));
    wrapped.push_back(0xa2);  // responseToken
    wrapped.push_back(static_cast<uint8_t>(token.size() + 2));
    wrapped.push_back(0x04);  // OCTET STRING
    wrapped.push_back(static_cast<uint8_t>(token.size()));
    wrapped.insert(wrapped.end(), token.begin(), token.end());

    return wrapped;
}

// ============================================================================
// Kerberos Utility Functions
// ============================================================================

bool parsePrincipal(const std::string& principal_str, KerberosPrincipal& principal) {
    principal = KerberosPrincipal{};
    principal.full_principal = principal_str;

    // Parse: primary[/instance][@REALM]
    size_t realm_pos = principal_str.rfind('@');
    std::string name_part;

    if (realm_pos != std::string::npos) {
        principal.realm = principal_str.substr(realm_pos + 1);
        name_part = principal_str.substr(0, realm_pos);
    } else {
        name_part = principal_str;
    }

    size_t slash_pos = name_part.find('/');
    if (slash_pos != std::string::npos) {
        principal.primary = name_part.substr(0, slash_pos);
        principal.instance = name_part.substr(slash_pos + 1);
        principal.is_service = true;
    } else {
        principal.primary = name_part;
    }

    return !principal.primary.empty();
}

std::string buildPrincipal(const std::string& primary,
                           const std::string& instance,
                           const std::string& realm) {
    std::string principal = primary;

    if (!instance.empty()) {
        principal += "/" + instance;
    }

    if (!realm.empty()) {
        principal += "@" + realm;
    }

    return principal;
}

std::string getDefaultRealm() {
    // In real implementation, would call krb5_get_default_realm
    // For now, try to read from environment or krb5.conf

    const char* realm_env = std::getenv("KRB5_REALM");
    if (realm_env) {
        return realm_env;
    }

    // Try to parse /etc/krb5.conf
    std::ifstream krb5_conf("/etc/krb5.conf");
    if (krb5_conf.is_open()) {
        std::string line;
        bool in_libdefaults = false;

        while (std::getline(krb5_conf, line)) {
            // Trim whitespace
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            line = line.substr(start);

            if (line[0] == '[') {
                in_libdefaults = (line.find("[libdefaults]") != std::string::npos);
                continue;
            }

            if (in_libdefaults && line.find("default_realm") != std::string::npos) {
                size_t eq_pos = line.find('=');
                if (eq_pos != std::string::npos) {
                    std::string realm = line.substr(eq_pos + 1);
                    // Trim whitespace
                    start = realm.find_first_not_of(" \t");
                    size_t end = realm.find_last_not_of(" \t\n\r");
                    if (start != std::string::npos) {
                        return realm.substr(start, end - start + 1);
                    }
                }
            }
        }
    }

    return "EXAMPLE.COM";
}

core::Status validateKeytab(const std::string& keytab_path,
                            const std::string& principal,
                            core::ErrorContext* ctx) {
    // Check file exists and is readable
    std::ifstream file(keytab_path, std::ios::binary);
    if (!file.good()) {
        if (ctx) ctx->message = "Keytab file not found: " + keytab_path;
        return core::Status::NOT_FOUND;
    }

    // Check keytab magic number
    uint16_t version;
    file.read(reinterpret_cast<char*>(&version), 2);
    if (!file.good()) {
        if (ctx) ctx->message = "Cannot read keytab file";
        return core::Status::IO_ERROR;
    }

    // Keytab version should be 0x0502 (big-endian) or 0x0501
    uint16_t version_be = (version >> 8) | (version << 8);  // Swap bytes
    if (version_be != 0x0502 && version_be != 0x0501 &&
        version != 0x0502 && version != 0x0501) {
        if (ctx) ctx->message = "Invalid keytab format";
        return core::Status::INVALID_ARGUMENT;
    }

    // In real implementation, would iterate entries to find principal
    return core::Status::OK;
}

core::Status listKeytabPrincipals(const std::string& keytab_path,
                                  std::vector<std::string>& principals,
                                  core::ErrorContext* ctx) {
    // Stub: In real implementation, would parse keytab entries
    // For now, return empty list
    principals.clear();

    auto status = validateKeytab(keytab_path, "", ctx);
    if (status != core::Status::OK) {
        return status;
    }

    // Would parse keytab and extract principal names
    return core::Status::OK;
}

core::Status gssErrorToStatus(uint32_t major_status,
                              uint32_t minor_status,
                              const std::string& operation) {
    // Map GSS major status codes to Status
    // GSS_S_COMPLETE = 0

    if (major_status == 0) {
        return core::Status::OK;
    }

    // Check calling errors (bits 24-31)
    uint32_t calling_error = (major_status >> 24) & 0xff;
    if (calling_error != 0) {
        return core::Status::INVALID_ARGUMENT;
    }

    // Check routine errors (bits 16-23)
    uint32_t routine_error = (major_status >> 16) & 0xff;
    switch (routine_error) {
        case 1:  // GSS_S_BAD_MECH
            return core::Status::NOT_SUPPORTED;
        case 2:  // GSS_S_BAD_NAME
            return core::Status::INVALID_ARGUMENT;
        case 3:  // GSS_S_BAD_NAMETYPE
            return core::Status::INVALID_ARGUMENT;
        case 4:  // GSS_S_BAD_BINDINGS
            return core::Status::PERMISSION_DENIED;
        case 5:  // GSS_S_BAD_STATUS
            return core::Status::INTERNAL_ERROR;
        case 6:  // GSS_S_BAD_SIG / GSS_S_BAD_MIC
            return core::Status::PERMISSION_DENIED;
        case 7:  // GSS_S_NO_CRED
            return core::Status::PERMISSION_DENIED;
        case 8:  // GSS_S_NO_CONTEXT
            return core::Status::INTERNAL_ERROR;
        case 9:  // GSS_S_DEFECTIVE_TOKEN
            return core::Status::PROTOCOL_VIOLATION;
        case 10: // GSS_S_DEFECTIVE_CREDENTIAL
            return core::Status::PERMISSION_DENIED;
        case 11: // GSS_S_CREDENTIALS_EXPIRED
            return core::Status::PERMISSION_DENIED;
        case 12: // GSS_S_CONTEXT_EXPIRED
            return core::Status::LOCK_TIMEOUT;
        case 13: // GSS_S_FAILURE
            return core::Status::INTERNAL_ERROR;
        default:
            return core::Status::INTERNAL_ERROR;
    }
}

std::string getGssErrorMessage(uint32_t major_status, uint32_t minor_status) {
    // Stub: In real implementation, would call gss_display_status
    std::ostringstream oss;
    oss << "GSSAPI error: major=0x" << std::hex << major_status
        << " minor=0x" << minor_status;
    return oss.str();
}

bool isGssapiAvailable() {
    // Check if GSSAPI libraries are available
    // In real implementation, would try to load libgssapi_krb5

    // For stub, always return true
    return true;
}

}  // namespace security
}  // namespace scratchbird

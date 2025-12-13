#pragma once

/**
 * ScratchBird Kerberos/GSSAPI Authentication
 *
 * Alpha 3 Phase 3.5: Security Suite - Enterprise
 *
 * Implements Kerberos authentication via GSSAPI with:
 * - Service principal configuration
 * - Keytab management
 * - SPNEGO/Negotiate support
 * - Credential delegation
 * - Principal to user mapping
 */

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <mutex>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/security/auth_method.h"

namespace scratchbird {
namespace security {

// ============================================================================
// Kerberos Configuration
// ============================================================================

/**
 * Kerberos configuration
 */
struct KerberosConfig {
    // Service principal
    std::string service_name;           // Service name (default: scratchbird)
    std::string service_hostname;       // Hostname for service principal
    std::string realm;                  // Kerberos realm (e.g., EXAMPLE.COM)

    // Keytab
    std::string keytab_file;            // Path to keytab file
    std::string keytab_principal;       // Specific principal in keytab (optional)

    // Authentication options
    bool include_realm_in_username = false;  // Include @REALM in username
    bool allow_delegation = false;           // Allow credential delegation
    bool constrained_delegation = false;     // Constrained delegation only

    // Principal mapping
    std::string username_attribute;     // Extract from principal (default: first component)
    std::map<std::string, std::string> principal_map;  // Principal -> username overrides

    // Timeouts
    uint32_t context_timeout_seconds = 300;  // GSSAPI context timeout
};

/**
 * Kerberos principal information
 */
struct KerberosPrincipal {
    std::string full_principal;         // user@REALM or service/host@REALM
    std::string primary;                // Primary component (user or service)
    std::string instance;               // Instance component (host or empty)
    std::string realm;                  // Realm
    bool is_service = false;            // Is this a service principal?
};

/**
 * GSSAPI security context state
 */
enum class GssContextState : uint8_t {
    INITIAL = 0,
    CONTEXT_IN_PROGRESS = 1,
    CONTEXT_ESTABLISHED = 2,
    CONTEXT_FAILED = 3
};

/**
 * GSSAPI context information
 */
struct GssContext {
    GssContextState state = GssContextState::INITIAL;
    KerberosPrincipal client_principal;
    KerberosPrincipal server_principal;
    bool mutual_auth = false;
    bool delegated_creds = false;
    std::vector<uint8_t> delegated_cred_handle;  // Opaque cred handle
    uint32_t lifetime_remaining = 0;
};

// ============================================================================
// Kerberos Authentication Method
// ============================================================================

/**
 * Kerberos/GSSAPI Authentication Method
 *
 * Implements Kerberos Single Sign-On via GSSAPI.
 */
class KerberosAuthMethod : public AuthMethod {
public:
    KerberosAuthMethod();
    ~KerberosAuthMethod();

    AuthType type() const override { return AuthType::KERBEROS; }
    const char* name() const override { return "gss"; }

    core::Status initialize(const std::map<std::string, std::string>& config,
                            core::ErrorContext* ctx = nullptr) override;

    AuthResult start(AuthContext& ctx) override;
    AuthResult continueAuth(AuthContext& ctx,
                            const std::vector<uint8_t>& data) override;
    void abort(AuthContext& ctx) override;

    /**
     * Check if suitable for connection (requires GSSAPI-capable client)
     */
    bool isSuitable(const ConnectionInfo& conn) const override;

    /**
     * Set Kerberos configuration
     */
    void setConfig(const KerberosConfig& config);
    const KerberosConfig& config() const { return config_; }

    /**
     * Map Kerberos principal to database username
     */
    std::string mapPrincipalToUser(const KerberosPrincipal& principal);

    /**
     * Add principal mapping rule
     */
    void addPrincipalMapping(const std::string& principal, const std::string& username);

    /**
     * Get service principal name
     */
    std::string getServicePrincipal() const;

private:
    /**
     * Process GSSAPI token
     */
    AuthResult processGssToken(AuthContext& ctx, const std::vector<uint8_t>& token);

    /**
     * Initialize GSSAPI server credentials from keytab
     */
    core::Status initializeServerCredentials();

    /**
     * Clean up GSSAPI context for a session
     */
    void cleanupContext(AuthContext& ctx);

    KerberosConfig config_;

    // Server credential handle (opaque, managed internally)
    struct GssCredential;
    std::unique_ptr<GssCredential> server_cred_;

    // Active contexts (keyed by AuthContext pointer)
    struct ContextState;
    std::map<AuthContext*, std::unique_ptr<ContextState>> contexts_;
    std::mutex contexts_mutex_;
};

// ============================================================================
// SPNEGO Authentication Method
// ============================================================================

/**
 * SPNEGO (Negotiate) Authentication Method
 *
 * SPNEGO wrapper for HTTP/Web authentication using Kerberos.
 * Used by browsers and web clients via WWW-Authenticate: Negotiate
 */
class SpnegoAuthMethod : public KerberosAuthMethod {
public:
    SpnegoAuthMethod();
    ~SpnegoAuthMethod();

    const char* name() const override { return "spnego"; }

    AuthResult start(AuthContext& ctx) override;
    AuthResult continueAuth(AuthContext& ctx,
                            const std::vector<uint8_t>& data) override;

    /**
     * Check if token is SPNEGO wrapped
     */
    static bool isSpnegoToken(const std::vector<uint8_t>& token);

    /**
     * Unwrap SPNEGO token to get inner mechanism token
     */
    static std::vector<uint8_t> unwrapSpnegoToken(const std::vector<uint8_t>& token);

    /**
     * Wrap mechanism token in SPNEGO
     */
    static std::vector<uint8_t> wrapSpnegoToken(const std::vector<uint8_t>& token,
                                                 bool is_initial = true);

private:
    bool expect_spnego_ = true;
};

// ============================================================================
// Kerberos Utility Functions
// ============================================================================

/**
 * Parse Kerberos principal string
 */
bool parsePrincipal(const std::string& principal_str, KerberosPrincipal& principal);

/**
 * Build principal string from components
 */
std::string buildPrincipal(const std::string& primary,
                           const std::string& instance,
                           const std::string& realm);

/**
 * Get default realm from krb5.conf
 */
std::string getDefaultRealm();

/**
 * Validate keytab file exists and is readable
 */
core::Status validateKeytab(const std::string& keytab_path,
                            const std::string& principal = "",
                            core::ErrorContext* ctx = nullptr);

/**
 * List principals in keytab
 */
core::Status listKeytabPrincipals(const std::string& keytab_path,
                                  std::vector<std::string>& principals,
                                  core::ErrorContext* ctx = nullptr);

/**
 * Convert GSSAPI error to status
 */
core::Status gssErrorToStatus(uint32_t major_status,
                              uint32_t minor_status,
                              const std::string& operation);

/**
 * Get GSSAPI error message
 */
std::string getGssErrorMessage(uint32_t major_status, uint32_t minor_status);

/**
 * Check if GSSAPI/Kerberos libraries are available
 */
bool isGssapiAvailable();

}  // namespace security
}  // namespace scratchbird

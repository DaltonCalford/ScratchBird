#pragma once

/**
 * ScratchBird TLS/SSL Configuration
 *
 * Alpha 3 Phase 3.4: Security Suite
 *
 * Provides:
 * - TLS server and client configuration
 * - Certificate verification options
 * - Protocol and cipher selection
 * - Certificate revocation (CRL/OCSP)
 */

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

// Forward declare OpenSSL types
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct x509_st X509;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct x509_store_ctx_st X509_STORE_CTX;

namespace scratchbird {
namespace security {

// ============================================================================
// TLS Protocol Versions
// ============================================================================

/**
 * TLS protocol versions
 */
enum class TLSVersion : uint8_t {
    TLS_1_0 = 0,    // Deprecated, not recommended
    TLS_1_1 = 1,    // Deprecated, not recommended
    TLS_1_2 = 2,    // Minimum recommended
    TLS_1_3 = 3     // Preferred
};

/**
 * Convert TLS version to string
 */
const char* tlsVersionToString(TLSVersion version);

/**
 * Parse TLS version from string
 */
bool parseTLSVersion(const std::string& str, TLSVersion& version);

// ============================================================================
// Certificate Verification
// ============================================================================

/**
 * Certificate verification mode
 */
enum class VerifyMode : uint8_t {
    NONE = 0,           // No client certificate required
    OPTIONAL = 1,       // Request but don't require
    REQUIRE = 2,        // Require valid client certificate
    REQUIRE_ONCE = 3    // Require on first connection only
};

/**
 * Certificate purpose
 */
enum class CertPurpose : uint8_t {
    SSL_CLIENT = 0,
    SSL_SERVER = 1,
    ANY = 2
};

/**
 * OCSP (Online Certificate Status Protocol) mode
 */
enum class OCSPMode : uint8_t {
    DISABLED = 0,       // No OCSP checking
    SOFT_FAIL = 1,      // Check but allow if unreachable
    HARD_FAIL = 2       // Require successful OCSP response
};

// ============================================================================
// TLS Configuration
// ============================================================================

/**
 * TLS server configuration
 */
struct TLSConfig {
    // Enable TLS
    bool enabled = false;

    // Certificate and key files
    std::string cert_file;          // Server certificate (PEM)
    std::string key_file;           // Server private key (PEM)
    std::string key_password;       // Private key password (optional)

    // Certificate chain
    std::string chain_file;         // Certificate chain (optional)

    // Certificate Authority
    std::string ca_file;            // CA certificate file
    std::string ca_path;            // CA certificate directory

    // Protocol versions
    TLSVersion min_version = TLSVersion::TLS_1_2;
    TLSVersion max_version = TLSVersion::TLS_1_3;

    // Cipher configuration
    std::string ciphers;            // OpenSSL cipher list (TLS 1.2)
    std::string ciphersuites;       // TLS 1.3 cipher suites
    bool prefer_server_ciphers = true;

    // Client certificate verification
    VerifyMode verify_mode = VerifyMode::NONE;
    int verify_depth = 10;          // Maximum certificate chain depth

    // Certificate Revocation
    std::string crl_file;           // Certificate Revocation List
    bool crl_check = false;         // Enable CRL checking
    bool crl_check_all = false;     // Check entire chain

    // OCSP
    OCSPMode ocsp_mode = OCSPMode::DISABLED;
    std::string ocsp_responder_url; // Override OCSP responder URL
    uint32_t ocsp_timeout_ms = 5000; // OCSP request timeout

    // Session configuration
    bool session_tickets = true;    // Enable session tickets
    uint32_t session_timeout = 300; // Session cache timeout (seconds)
    std::string session_cache_id;   // Session cache ID

    // DH parameters
    std::string dh_params_file;     // DH parameters file

    // ECDH
    std::string ecdh_curve;         // ECDH curve name (e.g., "prime256v1")

    // Security options
    bool compression = false;       // Disable by default (CRIME attack)
    bool renegotiation = false;     // Disable insecure renegotiation

    /**
     * Get default cipher list for TLS 1.2
     */
    static std::string getDefaultCiphers();

    /**
     * Get default cipher suites for TLS 1.3
     */
    static std::string getDefaultCipherSuites();

    /**
     * Validate configuration
     */
    core::Status validate(core::ErrorContext* ctx = nullptr) const;
};

/**
 * TLS client configuration (for outbound connections)
 */
struct TLSClientConfig {
    // Enable TLS
    bool enabled = true;

    // Server verification
    bool verify_server = true;      // Verify server certificate
    std::string expected_hostname;  // Expected server hostname

    // Client certificate (optional)
    std::string cert_file;          // Client certificate (PEM)
    std::string key_file;           // Client private key (PEM)
    std::string key_password;       // Private key password

    // Certificate Authority
    std::string ca_file;            // CA certificate file
    std::string ca_path;            // CA certificate directory
    bool use_system_ca = true;      // Use system CA store

    // Protocol versions
    TLSVersion min_version = TLSVersion::TLS_1_2;
    TLSVersion max_version = TLSVersion::TLS_1_3;

    // Cipher configuration
    std::string ciphers;            // OpenSSL cipher list
    std::string ciphersuites;       // TLS 1.3 cipher suites

    // SNI (Server Name Indication)
    bool use_sni = true;            // Enable SNI
    std::string sni_hostname;       // SNI hostname (if different)

    // OCSP stapling
    bool ocsp_stapling = false;     // Request OCSP stapling

    // Timeouts
    uint32_t connect_timeout_ms = 30000;
    uint32_t handshake_timeout_ms = 10000;

    /**
     * Validate configuration
     */
    core::Status validate(core::ErrorContext* ctx = nullptr) const;
};

// ============================================================================
// Certificate Information
// ============================================================================

/**
 * X.509 certificate information
 */
struct CertificateInfo {
    // Subject
    std::string subject_cn;         // Common Name
    std::string subject_dn;         // Full Distinguished Name
    std::string subject_o;          // Organization
    std::string subject_ou;         // Organizational Unit
    std::string subject_c;          // Country

    // Issuer
    std::string issuer_cn;          // Issuer Common Name
    std::string issuer_dn;          // Issuer Distinguished Name

    // Validity
    std::chrono::system_clock::time_point not_before;
    std::chrono::system_clock::time_point not_after;
    bool is_valid = false;

    // Serial number (hex string)
    std::string serial_number;

    // Fingerprints
    std::string fingerprint_sha1;   // SHA-1 fingerprint (hex)
    std::string fingerprint_sha256; // SHA-256 fingerprint (hex)

    // Subject Alternative Names
    std::vector<std::string> san_dns;    // DNS names
    std::vector<std::string> san_email;  // Email addresses
    std::vector<std::string> san_ip;     // IP addresses
    std::vector<std::string> san_uri;    // URIs

    // Key usage
    bool key_usage_digital_signature = false;
    bool key_usage_key_encipherment = false;
    bool key_usage_key_agreement = false;
    bool key_usage_cert_sign = false;
    bool key_usage_crl_sign = false;

    // Extended key usage
    bool ext_key_usage_server_auth = false;
    bool ext_key_usage_client_auth = false;

    // CA flag
    bool is_ca = false;
    int path_length = -1;           // -1 = no constraint

    /**
     * Check if certificate is currently valid
     */
    bool isCurrentlyValid() const;

    /**
     * Get days until expiration
     */
    int daysUntilExpiration() const;

    /**
     * Check if hostname matches certificate
     */
    bool matchesHostname(const std::string& hostname) const;
};

// ============================================================================
// Verification Callback
// ============================================================================

/**
 * Certificate verification result
 */
struct VerifyResult {
    bool verified = false;
    int depth = 0;                  // Depth in certificate chain
    int error_code = 0;             // OpenSSL error code
    std::string error_message;
    CertificateInfo cert_info;
};

/**
 * Custom verification callback
 * Return true to accept certificate, false to reject
 */
using VerifyCallback = std::function<bool(const VerifyResult&)>;

// ============================================================================
// TLS Context
// ============================================================================

/**
 * TLS context wrapper
 *
 * Manages OpenSSL SSL_CTX with proper initialization and cleanup.
 */
class TLSContext {
public:
    /**
     * Create server TLS context
     */
    static std::unique_ptr<TLSContext> createServer(
        const TLSConfig& config,
        core::ErrorContext* ctx = nullptr);

    /**
     * Create client TLS context
     */
    static std::unique_ptr<TLSContext> createClient(
        const TLSClientConfig& config,
        core::ErrorContext* ctx = nullptr);

    ~TLSContext();

    // Disable copy
    TLSContext(const TLSContext&) = delete;
    TLSContext& operator=(const TLSContext&) = delete;

    // Allow move
    TLSContext(TLSContext&& other) noexcept;
    TLSContext& operator=(TLSContext&& other) noexcept;

    /**
     * Get underlying SSL_CTX
     */
    SSL_CTX* get() const { return ctx_; }

    /**
     * Check if context is valid
     */
    bool isValid() const { return ctx_ != nullptr; }

    /**
     * Set custom verification callback
     */
    void setVerifyCallback(VerifyCallback callback);

    /**
     * Get server certificate info (for server contexts)
     */
    const CertificateInfo* getServerCertInfo() const { return &server_cert_info_; }

    /**
     * Reload certificates without recreating context
     */
    core::Status reloadCertificates(core::ErrorContext* ctx = nullptr);

private:
    TLSContext();

    core::Status initServer(const TLSConfig& config, core::ErrorContext* ctx);
    core::Status initClient(const TLSClientConfig& config, core::ErrorContext* ctx);

    core::Status loadCertificate(const std::string& cert_file,
                                  const std::string& key_file,
                                  const std::string& password,
                                  core::ErrorContext* ctx);

    core::Status loadCA(const std::string& ca_file,
                        const std::string& ca_path,
                        core::ErrorContext* ctx);

    core::Status loadCRL(const std::string& crl_file,
                         core::ErrorContext* ctx);

    core::Status configureCiphers(const std::string& ciphers,
                                   const std::string& ciphersuites,
                                   core::ErrorContext* ctx);

    core::Status configureProtocols(TLSVersion min_ver,
                                     TLSVersion max_ver,
                                     core::ErrorContext* ctx);

    static int verifyCallbackWrapper(int preverify_ok, X509_STORE_CTX* x509_ctx);

    SSL_CTX* ctx_ = nullptr;
    bool is_server_ = false;
    VerifyCallback verify_callback_;
    CertificateInfo server_cert_info_;

    // For certificate reload
    std::string cert_file_;
    std::string key_file_;
    std::string key_password_;
};

// ============================================================================
// TLS Connection
// ============================================================================

/**
 * TLS connection state
 */
enum class TLSState : uint8_t {
    INITIAL = 0,
    HANDSHAKE_IN_PROGRESS,
    ESTABLISHED,
    SHUTDOWN_IN_PROGRESS,
    SHUTDOWN_COMPLETE,
    ERROR
};

/**
 * TLS connection wrapper
 *
 * Wraps an OpenSSL SSL handle for a single connection.
 */
class TLSConnection {
public:
    /**
     * Create TLS connection from context
     */
    explicit TLSConnection(TLSContext& ctx);
    ~TLSConnection();

    // Disable copy
    TLSConnection(const TLSConnection&) = delete;
    TLSConnection& operator=(const TLSConnection&) = delete;

    /**
     * Set file descriptor for connection
     */
    core::Status setFd(int fd);

    /**
     * Set SNI hostname (for client connections)
     */
    core::Status setSNIHostname(const std::string& hostname);

    /**
     * Perform TLS handshake (server accept)
     * Returns OK on success, WOULD_BLOCK if non-blocking
     */
    core::Status accept();

    /**
     * Perform TLS handshake (client connect)
     * Returns OK on success, WOULD_BLOCK if non-blocking
     */
    core::Status connect();

    /**
     * Read data from TLS connection
     * @return bytes read, 0 on EOF, -1 on error (check getLastError)
     */
    int read(void* buffer, int size);

    /**
     * Write data to TLS connection
     * @return bytes written, -1 on error (check getLastError)
     */
    int write(const void* buffer, int size);

    /**
     * Initiate TLS shutdown
     */
    core::Status shutdown();

    /**
     * Get current TLS state
     */
    TLSState state() const { return state_; }

    /**
     * Get peer certificate information
     */
    const CertificateInfo* getPeerCertInfo() const { return &peer_cert_info_; }

    /**
     * Get negotiated protocol version
     */
    TLSVersion getVersion() const;

    /**
     * Get negotiated cipher suite name
     */
    std::string getCipherName() const;

    /**
     * Get last SSL error
     */
    int getLastError() const { return last_error_; }

    /**
     * Get last error message
     */
    std::string getLastErrorMessage() const;

    /**
     * Check if operation would block
     */
    bool wouldBlock() const { return want_read_ || want_write_; }

    /**
     * Check if waiting for read
     */
    bool wantRead() const { return want_read_; }

    /**
     * Check if waiting for write
     */
    bool wantWrite() const { return want_write_; }

    /**
     * Get underlying SSL handle
     */
    SSL* get() const { return ssl_; }

private:
    void extractPeerCertInfo();
    void handleSSLError(int ret);

    SSL* ssl_ = nullptr;
    TLSState state_ = TLSState::INITIAL;
    CertificateInfo peer_cert_info_;
    int last_error_ = 0;
    bool want_read_ = false;
    bool want_write_ = false;
};

// ============================================================================
// Certificate Utilities
// ============================================================================

/**
 * Extract certificate info from X509
 */
CertificateInfo extractCertificateInfo(X509* cert);

/**
 * Load certificate from file
 */
X509* loadCertificateFromFile(const std::string& path,
                              core::ErrorContext* ctx = nullptr);

/**
 * Load private key from file
 */
EVP_PKEY* loadPrivateKeyFromFile(const std::string& path,
                                  const std::string& password = "",
                                  core::ErrorContext* ctx = nullptr);

/**
 * Verify certificate chain
 */
core::Status verifyCertificateChain(X509* cert,
                                     const std::vector<X509*>& chain,
                                     const std::string& ca_file,
                                     core::ErrorContext* ctx = nullptr);

/**
 * Check if hostname matches certificate
 */
bool matchHostname(X509* cert, const std::string& hostname);

/**
 * Get certificate fingerprint (SHA-256)
 */
std::string getCertificateFingerprint(X509* cert);

// ============================================================================
// Global TLS Functions
// ============================================================================

/**
 * Initialize OpenSSL (call once at startup)
 */
void initializeOpenSSL();

/**
 * Cleanup OpenSSL (call once at shutdown)
 */
void cleanupOpenSSL();

/**
 * Get OpenSSL version string
 */
std::string getOpenSSLVersion();

/**
 * Check if OpenSSL supports a specific cipher
 */
bool isCipherSupported(const std::string& cipher);

/**
 * Check if OpenSSL supports a specific TLS version
 */
bool isTLSVersionSupported(TLSVersion version);

}  // namespace security
}  // namespace scratchbird

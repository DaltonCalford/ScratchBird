/**
 * ScratchBird TLS Context Implementation
 *
 * Alpha 3 Phase 3.4: Security Suite
 */

#include "scratchbird/security/tls_config.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/ocsp.h>

#include <mutex>
#include <atomic>
#include <cstring>
#include <ctime>
#include <sstream>
#include <iomanip>

// Network includes for IP address handling
#include <arpa/inet.h>
#include <netinet/in.h>

namespace scratchbird {
namespace security {

// ============================================================================
// Global State
// ============================================================================

static std::once_flag ssl_init_flag;
static std::atomic<bool> ssl_initialized{false};

// Thread-local for verify callback context
thread_local TLSContext* current_verify_context = nullptr;

// ============================================================================
// TLS Version Helpers
// ============================================================================

const char* tlsVersionToString(TLSVersion version) {
    switch (version) {
        case TLSVersion::TLS_1_0: return "TLSv1.0";
        case TLSVersion::TLS_1_1: return "TLSv1.1";
        case TLSVersion::TLS_1_2: return "TLSv1.2";
        case TLSVersion::TLS_1_3: return "TLSv1.3";
        default: return "Unknown";
    }
}

bool parseTLSVersion(const std::string& str, TLSVersion& version) {
    if (str == "TLSv1.0" || str == "TLS1.0" || str == "1.0") {
        version = TLSVersion::TLS_1_0;
        return true;
    } else if (str == "TLSv1.1" || str == "TLS1.1" || str == "1.1") {
        version = TLSVersion::TLS_1_1;
        return true;
    } else if (str == "TLSv1.2" || str == "TLS1.2" || str == "1.2") {
        version = TLSVersion::TLS_1_2;
        return true;
    } else if (str == "TLSv1.3" || str == "TLS1.3" || str == "1.3") {
        version = TLSVersion::TLS_1_3;
        return true;
    }
    return false;
}

static int tlsVersionToSSL(TLSVersion version) {
    switch (version) {
        case TLSVersion::TLS_1_0: return TLS1_VERSION;
        case TLSVersion::TLS_1_1: return TLS1_1_VERSION;
        case TLSVersion::TLS_1_2: return TLS1_2_VERSION;
        case TLSVersion::TLS_1_3: return TLS1_3_VERSION;
        default: return TLS1_2_VERSION;
    }
}

static TLSVersion sslVersionToTLS(int version) {
    switch (version) {
        case TLS1_VERSION: return TLSVersion::TLS_1_0;
        case TLS1_1_VERSION: return TLSVersion::TLS_1_1;
        case TLS1_2_VERSION: return TLSVersion::TLS_1_2;
        case TLS1_3_VERSION: return TLSVersion::TLS_1_3;
        default: return TLSVersion::TLS_1_2;
    }
}

// ============================================================================
// OpenSSL Error Helpers
// ============================================================================

static std::string getOpenSSLErrorString() {
    unsigned long err = ERR_get_error();
    if (err == 0) {
        return "No error";
    }

    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    return std::string(buf);
}

static void clearOpenSSLErrors() {
    while (ERR_get_error() != 0) {}
}

// ============================================================================
// TLSConfig Implementation
// ============================================================================

std::string TLSConfig::getDefaultCiphers() {
    // Strong cipher list for TLS 1.2
    return "ECDHE+AESGCM:DHE+AESGCM:ECDHE+CHACHA20:DHE+CHACHA20:"
           "ECDHE+AES256:DHE+AES256:ECDHE+AES128:DHE+AES128:"
           "!aNULL:!eNULL:!EXPORT:!DES:!RC4:!3DES:!MD5:!PSK";
}

std::string TLSConfig::getDefaultCipherSuites() {
    // TLS 1.3 cipher suites
    return "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:"
           "TLS_AES_128_GCM_SHA256";
}

core::Status TLSConfig::validate(core::ErrorContext* ctx) const {
    if (!enabled) {
        return core::Status::OK;
    }

    // Certificate required
    if (cert_file.empty()) {
        if (ctx) ctx->message = "TLS certificate file required";
        return core::Status::INVALID_ARGUMENT;
    }

    // Key required
    if (key_file.empty()) {
        if (ctx) ctx->message = "TLS private key file required";
        return core::Status::INVALID_ARGUMENT;
    }

    // Version validation
    if (min_version > max_version) {
        if (ctx) ctx->message = "min_version cannot be greater than max_version";
        return core::Status::INVALID_ARGUMENT;
    }

    // Warn about deprecated versions
    if (min_version < TLSVersion::TLS_1_2) {
        // Just a warning, not an error
    }

    // Verify mode validation
    if (verify_mode != VerifyMode::NONE) {
        if (ca_file.empty() && ca_path.empty()) {
            if (ctx) ctx->message = "CA file or path required for client verification";
            return core::Status::INVALID_ARGUMENT;
        }
    }

    return core::Status::OK;
}

core::Status TLSClientConfig::validate(core::ErrorContext* ctx) const {
    if (!enabled) {
        return core::Status::OK;
    }

    // Version validation
    if (min_version > max_version) {
        if (ctx) ctx->message = "min_version cannot be greater than max_version";
        return core::Status::INVALID_ARGUMENT;
    }

    // If verify_server is true, need CA
    if (verify_server && ca_file.empty() && ca_path.empty() && !use_system_ca) {
        if (ctx) ctx->message = "CA file/path or system CA required for server verification";
        return core::Status::INVALID_ARGUMENT;
    }

    return core::Status::OK;
}

// ============================================================================
// CertificateInfo Implementation
// ============================================================================

bool CertificateInfo::isCurrentlyValid() const {
    auto now = std::chrono::system_clock::now();
    return now >= not_before && now <= not_after;
}

int CertificateInfo::daysUntilExpiration() const {
    auto now = std::chrono::system_clock::now();
    if (now > not_after) {
        return 0;
    }
    auto diff = std::chrono::duration_cast<std::chrono::hours>(not_after - now);
    return static_cast<int>(diff.count() / 24);
}

bool CertificateInfo::matchesHostname(const std::string& hostname) const {
    // Check CN
    if (subject_cn == hostname) {
        return true;
    }

    // Check SANs
    for (const auto& san : san_dns) {
        if (san == hostname) {
            return true;
        }
        // Wildcard matching
        if (san.size() > 2 && san[0] == '*' && san[1] == '.') {
            // *.example.com matches foo.example.com
            std::string suffix = san.substr(1);
            size_t dot_pos = hostname.find('.');
            if (dot_pos != std::string::npos) {
                std::string hostname_suffix = hostname.substr(dot_pos);
                if (hostname_suffix == suffix) {
                    return true;
                }
            }
        }
    }

    // Check IP addresses
    for (const auto& ip : san_ip) {
        if (ip == hostname) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// Certificate Info Extraction
// ============================================================================

static std::string extractDNComponent(X509_NAME* name, int nid) {
    int idx = X509_NAME_get_index_by_NID(name, nid, -1);
    if (idx < 0) {
        return "";
    }

    X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, idx);
    if (!entry) {
        return "";
    }

    ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
    if (!data) {
        return "";
    }

    unsigned char* utf8 = nullptr;
    int len = ASN1_STRING_to_UTF8(&utf8, data);
    if (len < 0 || !utf8) {
        return "";
    }

    std::string result(reinterpret_cast<char*>(utf8), len);
    OPENSSL_free(utf8);
    return result;
}

static std::string extractDN(X509_NAME* name) {
    if (!name) {
        return "";
    }

    BIO* bio = BIO_new(BIO_s_mem());
    if (!bio) {
        return "";
    }

    X509_NAME_print_ex(bio, name, 0, XN_FLAG_RFC2253);

    char* buf = nullptr;
    long len = BIO_get_mem_data(bio, &buf);
    std::string result(buf, len);
    BIO_free(bio);

    return result;
}

static std::chrono::system_clock::time_point asnTimeToTimePoint(const ASN1_TIME* asn_time) {
    if (!asn_time) {
        return {};
    }

    struct tm tm = {};
    if (ASN1_TIME_to_tm(asn_time, &tm) != 1) {
        return {};
    }

    time_t t = timegm(&tm);
    return std::chrono::system_clock::from_time_t(t);
}

static std::string bytesToHex(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; i++) {
        if (i > 0) oss << ':';
        oss << std::hex << std::setfill('0') << std::setw(2)
            << static_cast<int>(data[i]);
    }
    return oss.str();
}

CertificateInfo extractCertificateInfo(X509* cert) {
    CertificateInfo info;

    if (!cert) {
        return info;
    }

    // Subject
    X509_NAME* subject = X509_get_subject_name(cert);
    if (subject) {
        info.subject_cn = extractDNComponent(subject, NID_commonName);
        info.subject_dn = extractDN(subject);
        info.subject_o = extractDNComponent(subject, NID_organizationName);
        info.subject_ou = extractDNComponent(subject, NID_organizationalUnitName);
        info.subject_c = extractDNComponent(subject, NID_countryName);
    }

    // Issuer
    X509_NAME* issuer = X509_get_issuer_name(cert);
    if (issuer) {
        info.issuer_cn = extractDNComponent(issuer, NID_commonName);
        info.issuer_dn = extractDN(issuer);
    }

    // Validity
    info.not_before = asnTimeToTimePoint(X509_get0_notBefore(cert));
    info.not_after = asnTimeToTimePoint(X509_get0_notAfter(cert));
    info.is_valid = info.isCurrentlyValid();

    // Serial number
    ASN1_INTEGER* serial = X509_get_serialNumber(cert);
    if (serial) {
        BIGNUM* bn = ASN1_INTEGER_to_BN(serial, nullptr);
        if (bn) {
            char* hex = BN_bn2hex(bn);
            if (hex) {
                info.serial_number = hex;
                OPENSSL_free(hex);
            }
            BN_free(bn);
        }
    }

    // Fingerprints
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    if (X509_digest(cert, EVP_sha1(), md, &md_len)) {
        info.fingerprint_sha1 = bytesToHex(md, md_len);
    }

    if (X509_digest(cert, EVP_sha256(), md, &md_len)) {
        info.fingerprint_sha256 = bytesToHex(md, md_len);
    }

    // Subject Alternative Names
    GENERAL_NAMES* sans = static_cast<GENERAL_NAMES*>(
        X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr));
    if (sans) {
        int count = sk_GENERAL_NAME_num(sans);
        for (int i = 0; i < count; i++) {
            GENERAL_NAME* san = sk_GENERAL_NAME_value(sans, i);
            if (!san) continue;

            switch (san->type) {
                case GEN_DNS: {
                    const char* data = reinterpret_cast<const char*>(
                        ASN1_STRING_get0_data(san->d.dNSName));
                    int len = ASN1_STRING_length(san->d.dNSName);
                    info.san_dns.emplace_back(data, len);
                    break;
                }
                case GEN_EMAIL: {
                    const char* data = reinterpret_cast<const char*>(
                        ASN1_STRING_get0_data(san->d.rfc822Name));
                    int len = ASN1_STRING_length(san->d.rfc822Name);
                    info.san_email.emplace_back(data, len);
                    break;
                }
                case GEN_IPADD: {
                    const unsigned char* ip = ASN1_STRING_get0_data(san->d.iPAddress);
                    int len = ASN1_STRING_length(san->d.iPAddress);
                    if (len == 4) {
                        // IPv4
                        char buf[INET_ADDRSTRLEN];
                        snprintf(buf, sizeof(buf), "%d.%d.%d.%d",
                                 ip[0], ip[1], ip[2], ip[3]);
                        info.san_ip.emplace_back(buf);
                    } else if (len == 16) {
                        // IPv6
                        char buf[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, ip, buf, sizeof(buf));
                        info.san_ip.emplace_back(buf);
                    }
                    break;
                }
                case GEN_URI: {
                    const char* data = reinterpret_cast<const char*>(
                        ASN1_STRING_get0_data(san->d.uniformResourceIdentifier));
                    int len = ASN1_STRING_length(san->d.uniformResourceIdentifier);
                    info.san_uri.emplace_back(data, len);
                    break;
                }
            }
        }
        GENERAL_NAMES_free(sans);
    }

    // Key Usage
    ASN1_BIT_STRING* ku = static_cast<ASN1_BIT_STRING*>(
        X509_get_ext_d2i(cert, NID_key_usage, nullptr, nullptr));
    if (ku) {
        info.key_usage_digital_signature = ASN1_BIT_STRING_get_bit(ku, 0);
        info.key_usage_key_encipherment = ASN1_BIT_STRING_get_bit(ku, 2);
        info.key_usage_key_agreement = ASN1_BIT_STRING_get_bit(ku, 4);
        info.key_usage_cert_sign = ASN1_BIT_STRING_get_bit(ku, 5);
        info.key_usage_crl_sign = ASN1_BIT_STRING_get_bit(ku, 6);
        ASN1_BIT_STRING_free(ku);
    }

    // Extended Key Usage
    EXTENDED_KEY_USAGE* eku = static_cast<EXTENDED_KEY_USAGE*>(
        X509_get_ext_d2i(cert, NID_ext_key_usage, nullptr, nullptr));
    if (eku) {
        int count = sk_ASN1_OBJECT_num(eku);
        for (int i = 0; i < count; i++) {
            ASN1_OBJECT* obj = sk_ASN1_OBJECT_value(eku, i);
            int nid = OBJ_obj2nid(obj);
            if (nid == NID_server_auth) {
                info.ext_key_usage_server_auth = true;
            } else if (nid == NID_client_auth) {
                info.ext_key_usage_client_auth = true;
            }
        }
        EXTENDED_KEY_USAGE_free(eku);
    }

    // Basic Constraints
    BASIC_CONSTRAINTS* bc = static_cast<BASIC_CONSTRAINTS*>(
        X509_get_ext_d2i(cert, NID_basic_constraints, nullptr, nullptr));
    if (bc) {
        info.is_ca = bc->ca != 0;
        if (bc->pathlen) {
            info.path_length = ASN1_INTEGER_get(bc->pathlen);
        }
        BASIC_CONSTRAINTS_free(bc);
    }

    return info;
}

// ============================================================================
// TLSContext Implementation
// ============================================================================

TLSContext::TLSContext() = default;

TLSContext::~TLSContext() {
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
}

TLSContext::TLSContext(TLSContext&& other) noexcept
    : ctx_(other.ctx_)
    , is_server_(other.is_server_)
    , verify_callback_(std::move(other.verify_callback_))
    , server_cert_info_(std::move(other.server_cert_info_))
    , cert_file_(std::move(other.cert_file_))
    , key_file_(std::move(other.key_file_))
    , key_password_(std::move(other.key_password_))
{
    other.ctx_ = nullptr;
}

TLSContext& TLSContext::operator=(TLSContext&& other) noexcept {
    if (this != &other) {
        if (ctx_) {
            SSL_CTX_free(ctx_);
        }
        ctx_ = other.ctx_;
        is_server_ = other.is_server_;
        verify_callback_ = std::move(other.verify_callback_);
        server_cert_info_ = std::move(other.server_cert_info_);
        cert_file_ = std::move(other.cert_file_);
        key_file_ = std::move(other.key_file_);
        key_password_ = std::move(other.key_password_);
        other.ctx_ = nullptr;
    }
    return *this;
}

std::unique_ptr<TLSContext> TLSContext::createServer(
    const TLSConfig& config,
    core::ErrorContext* ctx)
{
    // Validate configuration first
    auto status = config.validate(ctx);
    if (status != core::Status::OK) {
        return nullptr;
    }

    // Ensure OpenSSL is initialized
    initializeOpenSSL();

    auto tls_ctx = std::unique_ptr<TLSContext>(new TLSContext());
    status = tls_ctx->initServer(config, ctx);
    if (status != core::Status::OK) {
        return nullptr;
    }

    return tls_ctx;
}

std::unique_ptr<TLSContext> TLSContext::createClient(
    const TLSClientConfig& config,
    core::ErrorContext* ctx)
{
    // Validate configuration first
    auto status = config.validate(ctx);
    if (status != core::Status::OK) {
        return nullptr;
    }

    // Ensure OpenSSL is initialized
    initializeOpenSSL();

    auto tls_ctx = std::unique_ptr<TLSContext>(new TLSContext());
    status = tls_ctx->initClient(config, ctx);
    if (status != core::Status::OK) {
        return nullptr;
    }

    return tls_ctx;
}

core::Status TLSContext::initServer(const TLSConfig& config, core::ErrorContext* ctx) {
    is_server_ = true;
    clearOpenSSLErrors();

    // Create SSL context
    const SSL_METHOD* method = TLS_server_method();
    ctx_ = SSL_CTX_new(method);
    if (!ctx_) {
        if (ctx) ctx->message = "Failed to create SSL_CTX: " + getOpenSSLErrorString();
        return core::Status::INTERNAL_ERROR;
    }

    // Configure protocol versions
    auto status = configureProtocols(config.min_version, config.max_version, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    // Configure ciphers
    std::string ciphers = config.ciphers.empty() ?
        TLSConfig::getDefaultCiphers() : config.ciphers;
    std::string ciphersuites = config.ciphersuites.empty() ?
        TLSConfig::getDefaultCipherSuites() : config.ciphersuites;
    status = configureCiphers(ciphers, ciphersuites, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    // Load certificate and key
    cert_file_ = config.cert_file;
    key_file_ = config.key_file;
    key_password_ = config.key_password;
    status = loadCertificate(config.cert_file, config.key_file, config.key_password, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    // Load certificate chain
    if (!config.chain_file.empty()) {
        if (SSL_CTX_use_certificate_chain_file(ctx_, config.chain_file.c_str()) != 1) {
            if (ctx) ctx->message = "Failed to load certificate chain: " + getOpenSSLErrorString();
            return core::Status::INVALID_ARGUMENT;
        }
    }

    // Load CA for client verification
    if (!config.ca_file.empty() || !config.ca_path.empty()) {
        status = loadCA(config.ca_file, config.ca_path, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    // Configure client verification
    int verify_flags = SSL_VERIFY_NONE;
    switch (config.verify_mode) {
        case VerifyMode::NONE:
            verify_flags = SSL_VERIFY_NONE;
            break;
        case VerifyMode::OPTIONAL:
            verify_flags = SSL_VERIFY_PEER;
            break;
        case VerifyMode::REQUIRE:
            verify_flags = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
            break;
        case VerifyMode::REQUIRE_ONCE:
            verify_flags = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT |
                          SSL_VERIFY_CLIENT_ONCE;
            break;
    }
    SSL_CTX_set_verify(ctx_, verify_flags, nullptr);
    SSL_CTX_set_verify_depth(ctx_, config.verify_depth);

    // Load CRL
    if (config.crl_check && !config.crl_file.empty()) {
        status = loadCRL(config.crl_file, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    // DH parameters
    if (!config.dh_params_file.empty()) {
        FILE* fp = fopen(config.dh_params_file.c_str(), "r");
        if (fp) {
            DH* dh = PEM_read_DHparams(fp, nullptr, nullptr, nullptr);
            fclose(fp);
            if (dh) {
                SSL_CTX_set_tmp_dh(ctx_, dh);
                DH_free(dh);
            }
        }
    }

    // ECDH curve
    std::string curve = config.ecdh_curve.empty() ? "prime256v1" : config.ecdh_curve;
    int nid = OBJ_sn2nid(curve.c_str());
    if (nid != NID_undef) {
        EC_KEY* ecdh = EC_KEY_new_by_curve_name(nid);
        if (ecdh) {
            SSL_CTX_set_tmp_ecdh(ctx_, ecdh);
            EC_KEY_free(ecdh);
        }
    }

    // Security options
    long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
    if (!config.compression) {
        options |= SSL_OP_NO_COMPRESSION;
    }
    if (config.prefer_server_ciphers) {
        options |= SSL_OP_CIPHER_SERVER_PREFERENCE;
    }
    if (!config.renegotiation) {
        options |= SSL_OP_NO_RENEGOTIATION;
    }
    if (!config.session_tickets) {
        options |= SSL_OP_NO_TICKET;
    }
    SSL_CTX_set_options(ctx_, options);

    // Session cache
    if (!config.session_cache_id.empty()) {
        SSL_CTX_set_session_id_context(ctx_,
            reinterpret_cast<const unsigned char*>(config.session_cache_id.c_str()),
            static_cast<unsigned int>(config.session_cache_id.length()));
    }
    SSL_CTX_set_timeout(ctx_, config.session_timeout);

    // Extract server certificate info
    X509* cert = SSL_CTX_get0_certificate(ctx_);
    if (cert) {
        server_cert_info_ = extractCertificateInfo(cert);
    }

    return core::Status::OK;
}

core::Status TLSContext::initClient(const TLSClientConfig& config, core::ErrorContext* ctx) {
    is_server_ = false;
    clearOpenSSLErrors();

    // Create SSL context
    const SSL_METHOD* method = TLS_client_method();
    ctx_ = SSL_CTX_new(method);
    if (!ctx_) {
        if (ctx) ctx->message = "Failed to create SSL_CTX: " + getOpenSSLErrorString();
        return core::Status::INTERNAL_ERROR;
    }

    // Configure protocol versions
    auto status = configureProtocols(config.min_version, config.max_version, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    // Configure ciphers
    std::string ciphers = config.ciphers.empty() ?
        TLSConfig::getDefaultCiphers() : config.ciphers;
    std::string ciphersuites = config.ciphersuites.empty() ?
        TLSConfig::getDefaultCipherSuites() : config.ciphersuites;
    status = configureCiphers(ciphers, ciphersuites, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    // Load client certificate if specified
    if (!config.cert_file.empty()) {
        cert_file_ = config.cert_file;
        key_file_ = config.key_file;
        key_password_ = config.key_password;
        status = loadCertificate(config.cert_file, config.key_file, config.key_password, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    // Configure server verification
    if (config.verify_server) {
        SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);

        // Load CA
        if (config.use_system_ca) {
            if (SSL_CTX_set_default_verify_paths(ctx_) != 1) {
                if (ctx) ctx->message = "Failed to load system CA: " + getOpenSSLErrorString();
                return core::Status::INTERNAL_ERROR;
            }
        }

        if (!config.ca_file.empty() || !config.ca_path.empty()) {
            status = loadCA(config.ca_file, config.ca_path, ctx);
            if (status != core::Status::OK) {
                return status;
            }
        }
    } else {
        SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, nullptr);
    }

    // Security options
    long options = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION;
    SSL_CTX_set_options(ctx_, options);

    return core::Status::OK;
}

core::Status TLSContext::loadCertificate(
    const std::string& cert_file,
    const std::string& key_file,
    const std::string& password,
    core::ErrorContext* ctx)
{
    clearOpenSSLErrors();

    // Load certificate
    if (SSL_CTX_use_certificate_file(ctx_, cert_file.c_str(), SSL_FILETYPE_PEM) != 1) {
        if (ctx) ctx->message = "Failed to load certificate: " + getOpenSSLErrorString();
        return core::Status::INVALID_ARGUMENT;
    }

    // Set password callback if needed
    if (!password.empty()) {
        SSL_CTX_set_default_passwd_cb_userdata(ctx_,
            const_cast<char*>(password.c_str()));
        SSL_CTX_set_default_passwd_cb(ctx_, [](char* buf, int size, int, void* ud) -> int {
            const char* pw = static_cast<const char*>(ud);
            int len = static_cast<int>(strlen(pw));
            if (len > size) len = size;
            memcpy(buf, pw, len);
            return len;
        });
    }

    // Load private key
    if (SSL_CTX_use_PrivateKey_file(ctx_, key_file.c_str(), SSL_FILETYPE_PEM) != 1) {
        if (ctx) ctx->message = "Failed to load private key: " + getOpenSSLErrorString();
        return core::Status::INVALID_ARGUMENT;
    }

    // Verify key matches certificate
    if (SSL_CTX_check_private_key(ctx_) != 1) {
        if (ctx) ctx->message = "Private key does not match certificate";
        return core::Status::INVALID_ARGUMENT;
    }

    return core::Status::OK;
}

core::Status TLSContext::loadCA(
    const std::string& ca_file,
    const std::string& ca_path,
    core::ErrorContext* ctx)
{
    clearOpenSSLErrors();

    const char* file = ca_file.empty() ? nullptr : ca_file.c_str();
    const char* path = ca_path.empty() ? nullptr : ca_path.c_str();

    if (SSL_CTX_load_verify_locations(ctx_, file, path) != 1) {
        if (ctx) ctx->message = "Failed to load CA: " + getOpenSSLErrorString();
        return core::Status::INVALID_ARGUMENT;
    }

    return core::Status::OK;
}

core::Status TLSContext::loadCRL(const std::string& crl_file, core::ErrorContext* ctx) {
    clearOpenSSLErrors();

    X509_STORE* store = SSL_CTX_get_cert_store(ctx_);
    if (!store) {
        if (ctx) ctx->message = "Failed to get certificate store";
        return core::Status::INTERNAL_ERROR;
    }

    // Load CRL
    FILE* fp = fopen(crl_file.c_str(), "r");
    if (!fp) {
        if (ctx) ctx->message = "Failed to open CRL file: " + crl_file;
        return core::Status::NOT_FOUND;
    }

    X509_CRL* crl = PEM_read_X509_CRL(fp, nullptr, nullptr, nullptr);
    fclose(fp);

    if (!crl) {
        if (ctx) ctx->message = "Failed to parse CRL: " + getOpenSSLErrorString();
        return core::Status::INVALID_ARGUMENT;
    }

    if (X509_STORE_add_crl(store, crl) != 1) {
        X509_CRL_free(crl);
        if (ctx) ctx->message = "Failed to add CRL to store: " + getOpenSSLErrorString();
        return core::Status::INTERNAL_ERROR;
    }

    X509_CRL_free(crl);

    // Enable CRL checking
    X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK);

    return core::Status::OK;
}

core::Status TLSContext::configureCiphers(
    const std::string& ciphers,
    const std::string& ciphersuites,
    core::ErrorContext* ctx)
{
    clearOpenSSLErrors();

    // TLS 1.2 and earlier ciphers
    if (!ciphers.empty()) {
        if (SSL_CTX_set_cipher_list(ctx_, ciphers.c_str()) != 1) {
            if (ctx) ctx->message = "Failed to set cipher list: " + getOpenSSLErrorString();
            return core::Status::INVALID_ARGUMENT;
        }
    }

    // TLS 1.3 ciphersuites
    if (!ciphersuites.empty()) {
        if (SSL_CTX_set_ciphersuites(ctx_, ciphersuites.c_str()) != 1) {
            if (ctx) ctx->message = "Failed to set TLS 1.3 ciphersuites: " + getOpenSSLErrorString();
            return core::Status::INVALID_ARGUMENT;
        }
    }

    return core::Status::OK;
}

core::Status TLSContext::configureProtocols(
    TLSVersion min_ver,
    TLSVersion max_ver,
    core::ErrorContext* ctx)
{
    clearOpenSSLErrors();

    if (SSL_CTX_set_min_proto_version(ctx_, tlsVersionToSSL(min_ver)) != 1) {
        if (ctx) ctx->message = "Failed to set minimum TLS version";
        return core::Status::INVALID_ARGUMENT;
    }

    if (SSL_CTX_set_max_proto_version(ctx_, tlsVersionToSSL(max_ver)) != 1) {
        if (ctx) ctx->message = "Failed to set maximum TLS version";
        return core::Status::INVALID_ARGUMENT;
    }

    return core::Status::OK;
}

void TLSContext::setVerifyCallback(VerifyCallback callback) {
    verify_callback_ = std::move(callback);

    if (verify_callback_) {
        SSL_CTX_set_verify(ctx_, SSL_CTX_get_verify_mode(ctx_), verifyCallbackWrapper);
        SSL_CTX_set_app_data(ctx_, this);
    }
}

int TLSContext::verifyCallbackWrapper(int preverify_ok, X509_STORE_CTX* x509_ctx) {
    SSL* ssl = static_cast<SSL*>(
        X509_STORE_CTX_get_ex_data(x509_ctx, SSL_get_ex_data_X509_STORE_CTX_idx()));
    if (!ssl) {
        return preverify_ok;
    }

    SSL_CTX* ctx = SSL_get_SSL_CTX(ssl);
    TLSContext* tls_ctx = static_cast<TLSContext*>(SSL_CTX_get_app_data(ctx));
    if (!tls_ctx || !tls_ctx->verify_callback_) {
        return preverify_ok;
    }

    // Build verify result
    VerifyResult result;
    result.verified = preverify_ok != 0;
    result.depth = X509_STORE_CTX_get_error_depth(x509_ctx);
    result.error_code = X509_STORE_CTX_get_error(x509_ctx);
    result.error_message = X509_verify_cert_error_string(result.error_code);

    X509* cert = X509_STORE_CTX_get_current_cert(x509_ctx);
    if (cert) {
        result.cert_info = extractCertificateInfo(cert);
    }

    // Call user callback
    bool accepted = tls_ctx->verify_callback_(result);
    return accepted ? 1 : 0;
}

core::Status TLSContext::reloadCertificates(core::ErrorContext* ctx) {
    if (cert_file_.empty() || key_file_.empty()) {
        return core::Status::OK;
    }

    return loadCertificate(cert_file_, key_file_, key_password_, ctx);
}

// ============================================================================
// TLSConnection Implementation
// ============================================================================

TLSConnection::TLSConnection(TLSContext& ctx) {
    ssl_ = SSL_new(ctx.get());
}

TLSConnection::~TLSConnection() {
    if (ssl_) {
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
}

core::Status TLSConnection::setFd(int fd) {
    if (!ssl_) {
        return core::Status::INVALID_ARGUMENT;
    }

    if (SSL_set_fd(ssl_, fd) != 1) {
        return core::Status::INTERNAL_ERROR;
    }

    return core::Status::OK;
}

core::Status TLSConnection::setSNIHostname(const std::string& hostname) {
    if (!ssl_) {
        return core::Status::INVALID_ARGUMENT;
    }

    if (SSL_set_tlsext_host_name(ssl_, hostname.c_str()) != 1) {
        return core::Status::INTERNAL_ERROR;
    }

    return core::Status::OK;
}

core::Status TLSConnection::accept() {
    if (!ssl_) {
        return core::Status::INVALID_ARGUMENT;
    }

    state_ = TLSState::HANDSHAKE_IN_PROGRESS;
    want_read_ = false;
    want_write_ = false;

    int ret = SSL_accept(ssl_);
    if (ret == 1) {
        state_ = TLSState::ESTABLISHED;
        extractPeerCertInfo();
        return core::Status::OK;
    }

    handleSSLError(ret);
    if (want_read_ || want_write_) {
        return core::Status::LOCK_TIMEOUT;
    }

    state_ = TLSState::ERROR;
    return core::Status::IO_ERROR;
}

core::Status TLSConnection::connect() {
    if (!ssl_) {
        return core::Status::INVALID_ARGUMENT;
    }

    state_ = TLSState::HANDSHAKE_IN_PROGRESS;
    want_read_ = false;
    want_write_ = false;

    int ret = SSL_connect(ssl_);
    if (ret == 1) {
        state_ = TLSState::ESTABLISHED;
        extractPeerCertInfo();
        return core::Status::OK;
    }

    handleSSLError(ret);
    if (want_read_ || want_write_) {
        return core::Status::LOCK_TIMEOUT;
    }

    state_ = TLSState::ERROR;
    return core::Status::IO_ERROR;
}

int TLSConnection::read(void* buffer, int size) {
    if (!ssl_ || state_ != TLSState::ESTABLISHED) {
        return -1;
    }

    want_read_ = false;
    want_write_ = false;

    int ret = SSL_read(ssl_, buffer, size);
    if (ret <= 0) {
        handleSSLError(ret);
        if (last_error_ == SSL_ERROR_ZERO_RETURN) {
            return 0;  // Clean shutdown
        }
        return -1;
    }

    return ret;
}

int TLSConnection::write(const void* buffer, int size) {
    if (!ssl_ || state_ != TLSState::ESTABLISHED) {
        return -1;
    }

    want_read_ = false;
    want_write_ = false;

    int ret = SSL_write(ssl_, buffer, size);
    if (ret <= 0) {
        handleSSLError(ret);
        return -1;
    }

    return ret;
}

core::Status TLSConnection::shutdown() {
    if (!ssl_) {
        return core::Status::INVALID_ARGUMENT;
    }

    state_ = TLSState::SHUTDOWN_IN_PROGRESS;
    want_read_ = false;
    want_write_ = false;

    int ret = SSL_shutdown(ssl_);
    if (ret == 1) {
        state_ = TLSState::SHUTDOWN_COMPLETE;
        return core::Status::OK;
    }

    if (ret == 0) {
        // Need to call again
        return core::Status::LOCK_TIMEOUT;
    }

    handleSSLError(ret);
    if (want_read_ || want_write_) {
        return core::Status::LOCK_TIMEOUT;
    }

    state_ = TLSState::ERROR;
    return core::Status::IO_ERROR;
}

TLSVersion TLSConnection::getVersion() const {
    if (!ssl_) {
        return TLSVersion::TLS_1_2;
    }
    return sslVersionToTLS(SSL_version(ssl_));
}

std::string TLSConnection::getCipherName() const {
    if (!ssl_) {
        return "";
    }
    const char* cipher = SSL_get_cipher_name(ssl_);
    return cipher ? cipher : "";
}

std::string TLSConnection::getLastErrorMessage() const {
    return getOpenSSLErrorString();
}

void TLSConnection::extractPeerCertInfo() {
    if (!ssl_) {
        return;
    }

    X509* peer_cert = SSL_get_peer_certificate(ssl_);
    if (peer_cert) {
        peer_cert_info_ = extractCertificateInfo(peer_cert);
        X509_free(peer_cert);
    }
}

void TLSConnection::handleSSLError(int ret) {
    last_error_ = SSL_get_error(ssl_, ret);
    want_read_ = false;
    want_write_ = false;

    switch (last_error_) {
        case SSL_ERROR_WANT_READ:
            want_read_ = true;
            break;
        case SSL_ERROR_WANT_WRITE:
            want_write_ = true;
            break;
        default:
            break;
    }
}

// ============================================================================
// Certificate Utilities
// ============================================================================

X509* loadCertificateFromFile(const std::string& path, core::ErrorContext* ctx) {
    FILE* fp = fopen(path.c_str(), "r");
    if (!fp) {
        if (ctx) ctx->message = "Failed to open certificate file: " + path;
        return nullptr;
    }

    X509* cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
    fclose(fp);

    if (!cert) {
        if (ctx) ctx->message = "Failed to parse certificate: " + getOpenSSLErrorString();
    }

    return cert;
}

EVP_PKEY* loadPrivateKeyFromFile(
    const std::string& path,
    const std::string& password,
    core::ErrorContext* ctx)
{
    FILE* fp = fopen(path.c_str(), "r");
    if (!fp) {
        if (ctx) ctx->message = "Failed to open private key file: " + path;
        return nullptr;
    }

    EVP_PKEY* key = PEM_read_PrivateKey(fp, nullptr, nullptr,
        password.empty() ? nullptr : const_cast<char*>(password.c_str()));
    fclose(fp);

    if (!key) {
        if (ctx) ctx->message = "Failed to parse private key: " + getOpenSSLErrorString();
    }

    return key;
}

bool matchHostname(X509* cert, const std::string& hostname) {
    if (!cert) {
        return false;
    }

    CertificateInfo info = extractCertificateInfo(cert);
    return info.matchesHostname(hostname);
}

std::string getCertificateFingerprint(X509* cert) {
    if (!cert) {
        return "";
    }

    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len;

    if (X509_digest(cert, EVP_sha256(), md, &md_len)) {
        return bytesToHex(md, md_len);
    }

    return "";
}

// ============================================================================
// Global TLS Functions
// ============================================================================

void initializeOpenSSL() {
    std::call_once(ssl_init_flag, []() {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();

        // Seed random number generator
        if (RAND_status() != 1) {
            // Try to seed from /dev/urandom
            RAND_poll();
        }

        ssl_initialized.store(true);
    });
}

void cleanupOpenSSL() {
    if (ssl_initialized.exchange(false)) {
        EVP_cleanup();
        ERR_free_strings();
        CRYPTO_cleanup_all_ex_data();
    }
}

std::string getOpenSSLVersion() {
    return SSLeay_version(SSLEAY_VERSION);
}

bool isCipherSupported(const std::string& cipher) {
    SSL_CTX* ctx = SSL_CTX_new(TLS_method());
    if (!ctx) {
        return false;
    }

    int result = SSL_CTX_set_cipher_list(ctx, cipher.c_str());
    SSL_CTX_free(ctx);

    return result == 1;
}

bool isTLSVersionSupported(TLSVersion version) {
    // TLS 1.0 and 1.1 are deprecated but might still be supported
    // TLS 1.2 and 1.3 should be supported by modern OpenSSL
    switch (version) {
        case TLSVersion::TLS_1_0:
        case TLSVersion::TLS_1_1:
#ifdef TLS1_VERSION
            return true;
#else
            return false;
#endif
        case TLSVersion::TLS_1_2:
#ifdef TLS1_2_VERSION
            return true;
#else
            return false;
#endif
        case TLSVersion::TLS_1_3:
#ifdef TLS1_3_VERSION
            return true;
#else
            return false;
#endif
        default:
            return false;
    }
}

}  // namespace security
}  // namespace scratchbird

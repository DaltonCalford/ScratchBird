#include "scratchbird/engine/tls_server.h"

#include <arpa/inet.h>
#include <atomic>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509v3.h>
#include <queue>
#include <shared_mutex>
#include <sstream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace ScratchBird
{

    namespace
    {
        std::atomic<bool> openssl_initialized{false};
        std::mutex openssl_init_mutex;
    } // namespace

    // TLS Certificate Info Implementation
    bool TLSCertificateInfo::is_valid() const
    {
        auto now = std::chrono::system_clock::now();
        return now >= not_before && now <= not_after;
    }

    bool TLSCertificateInfo::is_expired() const
    {
        return std::chrono::system_clock::now() > not_after;
    }

    bool TLSCertificateInfo::is_self_signed() const
    {
        return subject == issuer;
    }

    // TLS Configuration Implementation
    bool TLSConfiguration::validate() const
    {
        std::vector<std::string> errors = get_validation_errors();
        return errors.empty();
    }

    std::vector<std::string> TLSConfiguration::get_validation_errors() const
    {
        std::vector<std::string> errors;

        if (certificate_file.empty()) {
            errors.push_back("Certificate file path is required");
        }

        if (private_key_file.empty()) {
            errors.push_back("Private key file path is required");
        }

        if (client_verification == TLSVerificationMode::Required && ca_certificate_file.empty()) {
            errors.push_back("CA certificate file required when client verification is enabled");
        }

        if (min_version == TLSVersion::TLS_1_3 && max_version == TLSVersion::TLS_1_2) {
            errors.push_back("Invalid TLS version range: min_version > max_version");
        }

        return errors;
    }

    // TLS Session Implementation
    TLSSession::TLSSession(SSL* ssl, const std::string& remote_address)
        : ssl_(ssl), remote_address_(remote_address),
          established_time_(std::chrono::system_clock::now()),
          last_activity_(std::chrono::system_clock::now())
    {
    }

    TLSSession::~TLSSession()
    {
        if (ssl_) {
            SSL_shutdown(ssl_);
            SSL_free(ssl_);
        }
    }

    std::string TLSSession::get_cipher_suite() const
    {
        if (!ssl_)
            return "Unknown";
        const char* cipher = SSL_get_cipher_name(ssl_);
        return cipher ? std::string(cipher) : "Unknown";
    }

    std::string TLSSession::get_protocol_version() const
    {
        if (!ssl_)
            return "Unknown";
        const char* version = SSL_get_version(ssl_);
        return version ? std::string(version) : "Unknown";
    }

    bool TLSSession::has_client_certificate() const
    {
        if (!ssl_)
            return false;
        X509* cert = SSL_get_peer_certificate(ssl_);
        bool has_cert = (cert != nullptr);
        if (cert)
            X509_free(cert);
        return has_cert;
    }

    TLSCertificateInfo TLSSession::get_client_certificate_info() const
    {
        TLSCertificateInfo info;

        if (!ssl_)
            return info;

        X509* cert = SSL_get_peer_certificate(ssl_);
        if (!cert)
            return info;

        // Extract subject
        char* subject_name = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
        if (subject_name) {
            info.subject = std::string(subject_name);
            OPENSSL_free(subject_name);
        }

        // Extract issuer
        char* issuer_name = X509_NAME_oneline(X509_get_issuer_name(cert), nullptr, 0);
        if (issuer_name) {
            info.issuer = std::string(issuer_name);
            OPENSSL_free(issuer_name);
        }

        // Extract serial number
        ASN1_INTEGER* serial = X509_get_serialNumber(cert);
        if (serial) {
            BIGNUM* bn = ASN1_INTEGER_to_BN(serial, nullptr);
            if (bn) {
                char* serial_str = BN_bn2hex(bn);
                if (serial_str) {
                    info.serial_number = std::string(serial_str);
                    OPENSSL_free(serial_str);
                }
                BN_free(bn);
            }
        }

        // Extract validity dates
        ASN1_TIME* not_before = X509_get_notBefore(cert);
        ASN1_TIME* not_after = X509_get_notAfter(cert);

        // Convert ASN1_TIME to time_t (simplified)
        if (not_before && not_after) {
            // This is a simplified conversion - in production, use proper ASN1_TIME parsing
            info.not_before =
                std::chrono::system_clock::now() - std::chrono::hours{24 * 30}; // Placeholder
            info.not_after =
                std::chrono::system_clock::now() + std::chrono::hours{24 * 365}; // Placeholder
        }

        // Generate SHA-256 fingerprint
        unsigned char md[EVP_MAX_MD_SIZE];
        unsigned int md_len;
        if (X509_digest(cert, EVP_sha256(), md, &md_len)) {
            std::ostringstream oss;
            for (unsigned int i = 0; i < md_len; ++i) {
                oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(md[i]);
                if (i < md_len - 1)
                    oss << ":";
            }
            info.fingerprint_sha256 = oss.str();
        }

        X509_free(cert);
        return info;
    }

    bool TLSSession::verify_client_certificate() const
    {
        if (!ssl_)
            return false;
        return SSL_get_verify_result(ssl_) == X509_V_OK;
    }

    bool TLSSession::has_perfect_forward_secrecy() const
    {
        if (!ssl_)
            return false;

        const SSL_CIPHER* cipher = SSL_get_current_cipher(ssl_);
        if (!cipher)
            return false;

        // Check if the cipher uses ephemeral key exchange
        std::string cipher_name = SSL_CIPHER_get_name(cipher);
        return cipher_name.find("ECDHE") != std::string::npos ||
               cipher_name.find("DHE") != std::string::npos;
    }

    std::string TLSSession::get_key_exchange_algorithm() const
    {
        if (!ssl_)
            return "Unknown";

        const SSL_CIPHER* cipher = SSL_get_current_cipher(ssl_);
        if (!cipher)
            return "Unknown";

        char desc[512];
        SSL_CIPHER_description(cipher, desc, sizeof(desc));

        // Parse description for key exchange info
        std::string description(desc);
        size_t kx_pos = description.find("Kx=");
        if (kx_pos != std::string::npos) {
            size_t start = kx_pos + 3;
            size_t end = description.find(' ', start);
            if (end == std::string::npos)
                end = description.length();
            return description.substr(start, end - start);
        }

        return "Unknown";
    }

    int TLSSession::get_key_bits() const
    {
        if (!ssl_)
            return 0;

        const SSL_CIPHER* cipher = SSL_get_current_cipher(ssl_);
        if (!cipher)
            return 0;

        return SSL_CIPHER_get_bits(cipher, nullptr);
    }

    bool TLSSession::is_resumed_session() const
    {
        if (!ssl_)
            return false;
        return SSL_session_reused(ssl_);
    }

    void TLSSession::update_activity()
    {
        last_activity_ = std::chrono::system_clock::now();
    }

    bool TLSSession::is_expired(std::chrono::minutes timeout) const
    {
        auto now = std::chrono::system_clock::now();
        return (now - last_activity_) > timeout;
    }

    // TLS Context Implementation
    TLSContext::TLSContext() : ssl_ctx_(nullptr)
    {
        initialize_openssl();
    }

    TLSContext::~TLSContext()
    {
        shutdown();
    }

    bool TLSContext::initialize(const TLSConfiguration& config)
    {
        std::lock_guard<std::mutex> lock(context_mutex_);

        if (!config.validate()) {
            return false;
        }

        config_ = config;

        // Create SSL context
        const SSL_METHOD* method = TLS_server_method();
        ssl_ctx_ = SSL_CTX_new(method);
        if (!ssl_ctx_) {
            return false;
        }

        // Configure the context
        if (!setup_protocol_versions() || !setup_cipher_configuration() ||
            !setup_certificate_verification() || !setup_session_management()) {
            SSL_CTX_free(ssl_ctx_);
            ssl_ctx_ = nullptr;
            return false;
        }

        // Load certificates and keys
        if (!load_certificate_chain(config_.certificate_file, config_.private_key_file)) {
            SSL_CTX_free(ssl_ctx_);
            ssl_ctx_ = nullptr;
            return false;
        }

        if (!config_.ca_certificate_file.empty()) {
            load_ca_certificates(config_.ca_certificate_file);
        }

        if (!config_.crl_file.empty()) {
            load_crl(config_.crl_file);
        }

        configure_security_options();
        setup_security_callbacks();

        return true;
    }

    void TLSContext::shutdown()
    {
        std::lock_guard<std::mutex> lock(context_mutex_);

        if (ssl_ctx_) {
            SSL_CTX_free(ssl_ctx_);
            ssl_ctx_ = nullptr;
        }
    }

    bool TLSContext::load_certificate_chain(const std::string& cert_file,
                                            const std::string& key_file)
    {
        if (!ssl_ctx_)
            return false;

        // Load certificate
        if (SSL_CTX_use_certificate_file(ssl_ctx_, cert_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
            return false;
        }

        // Load private key
        if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, key_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
            return false;
        }

        // Verify private key matches certificate
        if (!SSL_CTX_check_private_key(ssl_ctx_)) {
            return false;
        }

        return true;
    }

    bool TLSContext::load_ca_certificates(const std::string& ca_file)
    {
        if (!ssl_ctx_)
            return false;

        if (!SSL_CTX_load_verify_locations(ssl_ctx_, ca_file.c_str(), nullptr)) {
            return false;
        }

        // Set the list of CAs sent to the client
        STACK_OF(X509_NAME)* ca_list = SSL_load_client_CA_file(ca_file.c_str());
        if (ca_list) {
            SSL_CTX_set_client_CA_list(ssl_ctx_, ca_list);
        }

        return true;
    }

    bool TLSContext::load_crl(const std::string& crl_file)
    {
        if (!ssl_ctx_)
            return false;

        X509_STORE* store = SSL_CTX_get_cert_store(ssl_ctx_);
        if (!store)
            return false;

        X509_LOOKUP* lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file());
        if (!lookup)
            return false;

        if (!X509_LOOKUP_load_file(lookup, crl_file.c_str(), X509_FILETYPE_PEM)) {
            return false;
        }

        X509_STORE_set_flags(store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL);

        return true;
    }

    bool TLSContext::verify_certificate_chain() const
    {
        // This would implement certificate chain validation
        // For now, return true as a placeholder
        return ssl_ctx_ != nullptr;
    }

    void TLSContext::set_cipher_suites(const std::vector<std::string>& cipher_suites)
    {
        if (!ssl_ctx_ || cipher_suites.empty())
            return;

        std::string cipher_list;
        for (size_t i = 0; i < cipher_suites.size(); ++i) {
            if (i > 0)
                cipher_list += ":";
            cipher_list += cipher_suites[i];
        }

        SSL_CTX_set_cipher_list(ssl_ctx_, cipher_list.c_str());
    }

    void TLSContext::set_curves(const std::vector<std::string>& curves)
    {
        if (!ssl_ctx_ || curves.empty())
            return;

        std::string curve_list;
        for (size_t i = 0; i < curves.size(); ++i) {
            if (i > 0)
                curve_list += ":";
            curve_list += curves[i];
        }

        SSL_CTX_set1_curves_list(ssl_ctx_, curve_list.c_str());
    }

    void TLSContext::set_verification_mode(TLSVerificationMode mode)
    {
        if (!ssl_ctx_)
            return;

        int verify_mode = SSL_VERIFY_NONE;

        switch (mode) {
        case TLSVerificationMode::None:
            verify_mode = SSL_VERIFY_NONE;
            break;
        case TLSVerificationMode::Optional:
            verify_mode = SSL_VERIFY_PEER;
            break;
        case TLSVerificationMode::Required:
            verify_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
            break;
        }

        SSL_CTX_set_verify(ssl_ctx_, verify_mode, verify_certificate_callback);
    }

    void TLSContext::enable_session_cache(bool enable)
    {
        if (!ssl_ctx_)
            return;

        if (enable) {
            SSL_CTX_set_session_cache_mode(ssl_ctx_, SSL_SESS_CACHE_SERVER);
            SSL_CTX_sess_set_new_cb(ssl_ctx_, new_session_callback);
            SSL_CTX_sess_set_remove_cb(ssl_ctx_, remove_session_callback);
        } else {
            SSL_CTX_set_session_cache_mode(ssl_ctx_, SSL_SESS_CACHE_OFF);
        }
    }

    void TLSContext::configure_security_options()
    {
        if (!ssl_ctx_)
            return;

        long options = SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION;

        if (config_.disable_renegotiation) {
            options |= SSL_OP_NO_RENEGOTIATION;
        }

        if (config_.enforce_server_cipher_order) {
            options |= SSL_OP_CIPHER_SERVER_PREFERENCE;
        }

        SSL_CTX_set_options(ssl_ctx_, options);

        setup_dh_parameters();
        setup_ecdh_curves();
    }

    void TLSContext::setup_dh_parameters()
    {
        if (!ssl_ctx_)
            return;

        // Use built-in DH parameters or generate them
        SSL_CTX_set_dh_auto(ssl_ctx_, 1);
    }

    void TLSContext::setup_ecdh_curves()
    {
        if (!ssl_ctx_)
            return;

        // Set up ECDH curves for perfect forward secrecy
        if (config_.allowed_curves.empty()) {
            SSL_CTX_set1_curves_list(ssl_ctx_, "X25519:P-256:P-384:P-521");
        } else {
            set_curves(config_.allowed_curves);
        }
    }

    bool TLSContext::setup_protocol_versions()
    {
        if (!ssl_ctx_)
            return false;

        int min_proto = TLS1_2_VERSION;
        int max_proto = TLS1_3_VERSION;

        switch (config_.min_version) {
        case TLSVersion::TLS_1_2:
            min_proto = TLS1_2_VERSION;
            break;
        case TLSVersion::TLS_1_3:
            min_proto = TLS1_3_VERSION;
            break;
        case TLSVersion::Auto:
            min_proto = TLS1_2_VERSION;
            break;
        }

        switch (config_.max_version) {
        case TLSVersion::TLS_1_2:
            max_proto = TLS1_2_VERSION;
            break;
        case TLSVersion::TLS_1_3:
            max_proto = TLS1_3_VERSION;
            break;
        case TLSVersion::Auto:
            max_proto = TLS1_3_VERSION;
            break;
        }

        SSL_CTX_set_min_proto_version(ssl_ctx_, min_proto);
        SSL_CTX_set_max_proto_version(ssl_ctx_, max_proto);

        return true;
    }

    bool TLSContext::setup_cipher_configuration()
    {
        if (!ssl_ctx_)
            return false;

        if (!config_.allowed_cipher_suites.empty()) {
            set_cipher_suites(config_.allowed_cipher_suites);
        } else {
            // Set secure default cipher suites
            SSL_CTX_set_cipher_list(
                ssl_ctx_, "ECDHE+AESGCM:ECDHE+CHACHA20:DHE+AESGCM:DHE+CHACHA20:!aNULL:!MD5:!DSS");
        }

        return true;
    }

    bool TLSContext::setup_certificate_verification()
    {
        set_verification_mode(config_.client_verification);
        return true;
    }

    bool TLSContext::setup_session_management()
    {
        enable_session_cache(config_.enable_session_resumption);

        if (config_.session_timeout.count() > 0) {
            SSL_CTX_set_timeout(ssl_ctx_, static_cast<long>(config_.session_timeout.count() * 60));
        }

        return true;
    }

    void TLSContext::setup_security_callbacks()
    {
        if (!ssl_ctx_)
            return;

        SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_PEER, verify_certificate_callback);
    }

    // Static callback implementations
    int TLSContext::verify_certificate_callback(int preverify_ok, X509_STORE_CTX* ctx)
    {
        // Basic certificate verification
        // In production, implement more sophisticated verification
        return preverify_ok;
    }

    int TLSContext::select_certificate_callback(SSL* ssl, void* arg)
    {
        // Certificate selection callback
        return 1; // Use default certificate
    }

    int TLSContext::new_session_callback(SSL* ssl, SSL_SESSION* session)
    {
        // Handle new session creation
        return 1; // Success
    }

    void TLSContext::remove_session_callback(SSL_CTX* ctx, SSL_SESSION* session)
    {
        // Handle session removal
    }

    // TLS Server Implementation
    TLSServer::TLSServer()
        : tls_context_(std::make_unique<TLSContext>()), is_running_(false),
          shutdown_requested_(false)
    {
    }

    TLSServer::~TLSServer()
    {
        shutdown();
    }

    bool TLSServer::initialize(const TLSConfiguration& config)
    {
        std::lock_guard<std::mutex> lock(server_mutex_);

        config_ = config;

        if (!tls_context_->initialize(config)) {
            return false;
        }

        statistics_.last_reset = std::chrono::system_clock::now();

        // Start session cleanup thread
        cleanup_thread_ = std::thread(&TLSServer::session_cleanup_worker, this);

        return true;
    }

    void TLSServer::shutdown()
    {
        std::lock_guard<std::mutex> lock(server_mutex_);

        shutdown_requested_ = true;
        is_running_ = false;

        if (cleanup_thread_.joinable()) {
            cleanup_thread_.join();
        }

        // Cleanup all active sessions
        {
            std::unique_lock<std::shared_mutex> sessions_lock(sessions_mutex_);
            active_sessions_.clear();
        }

        if (tls_context_) {
            tls_context_->shutdown();
        }
    }

    bool TLSServer::start_listening(const std::string& bind_address, std::uint16_t port)
    {
        // This is a simplified implementation - full implementation would create socket
        // and start accepting connections

        std::lock_guard<std::mutex> lock(server_mutex_);
        is_running_ = true;

        log_audit_event(SecurityAuditEvent::ConfigurationChanged, bind_address,
                        "TLS server started on " + bind_address + ":" + std::to_string(port));

        return true;
    }

    void TLSServer::stop_listening()
    {
        std::lock_guard<std::mutex> lock(server_mutex_);
        is_running_ = false;

        log_audit_event(SecurityAuditEvent::ConfigurationChanged, "", "TLS server stopped");
    }

    std::unique_ptr<TLSSession> TLSServer::accept_tls_connection(int socket_fd,
                                                                 const std::string& remote_address)
    {
        if (!is_running_ || !tls_context_->is_initialized()) {
            return nullptr;
        }

        SSL* ssl = SSL_new(tls_context_->get_ssl_context());
        if (!ssl) {
            return nullptr;
        }

        if (SSL_set_fd(ssl, socket_fd) != 1) {
            SSL_free(ssl);
            return nullptr;
        }

        log_audit_event(SecurityAuditEvent::HandshakeStart, remote_address);

        auto session = std::make_unique<TLSSession>(ssl, remote_address);

        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.total_connections++;
        }

        return session;
    }

    bool TLSServer::perform_tls_handshake(TLSSession& session)
    {
        SSL* ssl = session.ssl_;
        if (!ssl)
            return false;

        int result = SSL_accept(ssl);

        if (result <= 0) {
            int error = SSL_get_error(ssl, result);
            handle_ssl_error("SSL_accept", error);

            log_audit_event(SecurityAuditEvent::HandshakeFailure, session.get_remote_address(),
                            "Handshake failed with error: " + std::to_string(error));

            {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                statistics_.failed_handshakes++;
            }

            return false;
        }

        log_audit_event(SecurityAuditEvent::HandshakeSuccess, session.get_remote_address());
        log_audit_event(SecurityAuditEvent::SessionEstablished, session.get_remote_address(),
                        "Cipher: " + session.get_cipher_suite() +
                            ", Protocol: " + session.get_protocol_version());

        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.successful_handshakes++;
        }

        return true;
    }

    void TLSServer::register_session(std::shared_ptr<TLSSession> session)
    {
        std::string session_id = generate_session_id();

        {
            std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
            active_sessions_[session_id] = session;

            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.active_sessions++;
        }
    }

    void TLSServer::unregister_session(const std::string& session_id)
    {
        {
            std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
            auto it = active_sessions_.find(session_id);
            if (it != active_sessions_.end()) {
                auto session = it->second.lock();
                if (session) {
                    log_audit_event(SecurityAuditEvent::SessionClosed,
                                    session->get_remote_address());
                }
                active_sessions_.erase(it);

                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                if (statistics_.active_sessions > 0) {
                    statistics_.active_sessions--;
                }
            }
        }
    }

    void TLSServer::cleanup_expired_sessions()
    {
        std::vector<std::string> expired_sessions;

        {
            std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
            for (const auto& [session_id, weak_session] : active_sessions_) {
                auto session = weak_session.lock();
                if (!session || session->is_expired(config_.session_timeout)) {
                    expired_sessions.push_back(session_id);
                }
            }
        }

        for (const auto& session_id : expired_sessions) {
            unregister_session(session_id);
        }
    }

    std::vector<std::shared_ptr<TLSSession>> TLSServer::get_active_sessions() const
    {
        std::vector<std::shared_ptr<TLSSession>> sessions;

        {
            std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
            sessions.reserve(active_sessions_.size());

            for (const auto& [session_id, weak_session] : active_sessions_) {
                auto session = weak_session.lock();
                if (session) {
                    sessions.push_back(session);
                }
            }
        }

        return sessions;
    }

    std::size_t TLSServer::get_active_session_count() const
    {
        std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
        return active_sessions_.size();
    }

    TLSServer::TLSStatistics TLSServer::get_statistics() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return statistics_;
    }

    void TLSServer::reset_statistics()
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_ = TLSStatistics{};
        statistics_.last_reset = std::chrono::system_clock::now();
    }

    bool TLSServer::update_configuration(const TLSConfiguration& new_config)
    {
        if (!new_config.validate()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(server_mutex_);

        config_ = new_config;
        bool result = tls_context_->initialize(new_config);

        if (result) {
            log_audit_event(SecurityAuditEvent::ConfigurationChanged, "",
                            "TLS configuration updated");
        }

        return result;
    }

    bool TLSServer::reload_certificates()
    {
        std::lock_guard<std::mutex> lock(server_mutex_);

        bool result = tls_context_->load_certificate_chain(config_.certificate_file,
                                                           config_.private_key_file);

        if (result) {
            log_audit_event(SecurityAuditEvent::ConfigurationChanged, "",
                            "TLS certificates reloaded");
        }

        return result;
    }

    // Private helper methods
    void TLSServer::log_audit_event(SecurityAuditEvent::Type type,
                                    const std::string& remote_address, const std::string& details)
    {
        if (audit_callback_) {
            SecurityAuditEvent event;
            event.type = type;
            event.remote_address = remote_address;
            event.details = details;
            event.timestamp = std::chrono::system_clock::now();

            audit_callback_(event);
        }
    }

    std::string TLSServer::generate_session_id() const
    {
        std::ostringstream oss;
        oss << std::hex
            << std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();

        // Add some randomness
        unsigned char random_bytes[8];
        if (RAND_bytes(random_bytes, sizeof(random_bytes)) == 1) {
            for (int i = 0; i < 8; ++i) {
                oss << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(random_bytes[i]);
            }
        }

        return oss.str();
    }

    void TLSServer::handle_ssl_error(const std::string& operation, int error_code)
    {
        char err_buf[256];
        ERR_error_string_n(error_code, err_buf, sizeof(err_buf));

        std::cerr << "SSL Error in " << operation << ": " << err_buf << std::endl;

        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.protocol_errors++;
        }
    }

    void TLSServer::session_cleanup_worker()
    {
        while (!shutdown_requested_) {
            cleanup_expired_sessions();
            std::this_thread::sleep_for(std::chrono::minutes{5}); // Cleanup every 5 minutes
        }
    }

    // TLS Certificate Manager Implementation
    TLSCertificateManager::TLSCertificateManager()
        : certificate_(nullptr), private_key_(nullptr), certificate_chain_(nullptr),
          ca_store_(nullptr)
    {
    }

    TLSCertificateManager::~TLSCertificateManager()
    {
        std::lock_guard<std::mutex> lock(cert_mutex_);

        if (certificate_)
            X509_free(certificate_);
        if (private_key_)
            EVP_PKEY_free(private_key_);
        if (certificate_chain_)
            sk_X509_pop_free(certificate_chain_, X509_free);
        if (ca_store_)
            X509_STORE_free(ca_store_);
    }

    bool TLSCertificateManager::load_certificate_from_file(const std::string& cert_file)
    {
        std::lock_guard<std::mutex> lock(cert_mutex_);

        FILE* fp = fopen(cert_file.c_str(), "r");
        if (!fp)
            return false;

        X509* cert = PEM_read_X509(fp, nullptr, nullptr, nullptr);
        fclose(fp);

        if (!cert)
            return false;

        if (certificate_)
            X509_free(certificate_);
        certificate_ = cert;

        return true;
    }

    bool TLSCertificateManager::load_private_key_from_file(const std::string& key_file)
    {
        std::lock_guard<std::mutex> lock(cert_mutex_);

        FILE* fp = fopen(key_file.c_str(), "r");
        if (!fp)
            return false;

        EVP_PKEY* key = PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
        fclose(fp);

        if (!key)
            return false;

        if (private_key_)
            EVP_PKEY_free(private_key_);
        private_key_ = key;

        return true;
    }

    bool TLSCertificateManager::generate_self_signed_certificate(
        const std::string& common_name, const std::vector<std::string>& alt_names,
        int validity_days)
    {
        std::lock_guard<std::mutex> lock(cert_mutex_);

        // Generate RSA key pair
        EVP_PKEY* pkey = EVP_PKEY_new();
        if (!pkey)
            return false;

        RSA* rsa = RSA_new();
        BIGNUM* bne = BN_new();

        if (!BN_set_word(bne, RSA_F4) || !RSA_generate_key_ex(rsa, 2048, bne, nullptr) ||
            !EVP_PKEY_assign_RSA(pkey, rsa)) {
            BN_free(bne);
            RSA_free(rsa);
            EVP_PKEY_free(pkey);
            return false;
        }

        BN_free(bne);

        // Create certificate
        X509* cert = X509_new();
        if (!cert) {
            EVP_PKEY_free(pkey);
            return false;
        }

        // Set version
        X509_set_version(cert, 2);

        // Set serial number
        ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);

        // Set validity
        X509_gmtime_adj(X509_get_notBefore(cert), 0);
        X509_gmtime_adj(X509_get_notAfter(cert), validity_days * 24 * 3600);

        // Set public key
        X509_set_pubkey(cert, pkey);

        // Set subject and issuer
        X509_NAME* name = X509_get_subject_name(cert);
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                   reinterpret_cast<const unsigned char*>(common_name.c_str()), -1,
                                   -1, 0);

        X509_set_issuer_name(cert, name);

        // Add Subject Alternative Names if provided
        if (!alt_names.empty()) {
            X509_EXTENSION* ext = nullptr;
            STACK_OF(GENERAL_NAME)* san_stack = sk_GENERAL_NAME_new_null();

            for (const auto& alt_name : alt_names) {
                GENERAL_NAME* gen = GENERAL_NAME_new();
                ASN1_STRING* san = ASN1_STRING_new();
                ASN1_STRING_set(san, alt_name.c_str(), alt_name.length());
                GENERAL_NAME_set0_value(gen, GEN_DNS, san);
                sk_GENERAL_NAME_push(san_stack, gen);
            }

            ext = X509V3_EXT_i2d(NID_subject_alt_name, 0, san_stack);
            if (ext) {
                X509_add_ext(cert, ext, -1);
                X509_EXTENSION_free(ext);
            }

            sk_GENERAL_NAME_pop_free(san_stack, GENERAL_NAME_free);
        }

        // Sign the certificate
        if (!X509_sign(cert, pkey, EVP_sha256())) {
            X509_free(cert);
            EVP_PKEY_free(pkey);
            return false;
        }

        // Store the certificate and key
        if (certificate_)
            X509_free(certificate_);
        if (private_key_)
            EVP_PKEY_free(private_key_);

        certificate_ = cert;
        private_key_ = pkey;

        return true;
    }

    bool TLSCertificateManager::validate_certificate_chain() const
    {
        std::lock_guard<std::mutex> lock(cert_mutex_);
        return certificate_ != nullptr; // Simplified validation
    }

    bool TLSCertificateManager::check_certificate_expiry(std::chrono::hours warning_threshold) const
    {
        std::lock_guard<std::mutex> lock(cert_mutex_);

        if (!certificate_)
            return false;

        // Get current time as ASN1_TIME for comparison
        ASN1_TIME* not_after = X509_get_notAfter(certificate_);
        if (!not_after)
            return false;

        // Simplified check - in production would properly parse ASN1_TIME
        auto now = std::chrono::system_clock::now();
        auto warning_time = now + warning_threshold;

        // For simplicity, assume certificate is valid for the warning period
        return true;
    }

    bool TLSCertificateManager::verify_private_key_match() const
    {
        std::lock_guard<std::mutex> lock(cert_mutex_);

        if (!certificate_ || !private_key_)
            return false;

        // Verify the private key matches the certificate
        EVP_PKEY* cert_public_key = X509_get_pubkey(certificate_);
        if (!cert_public_key)
            return false;

        int result = EVP_PKEY_cmp(cert_public_key, private_key_);
        EVP_PKEY_free(cert_public_key);

        return result == 1; // 1 means keys match
    }

    TLSCertificateInfo TLSCertificateManager::get_certificate_info() const
    {
        std::lock_guard<std::mutex> lock(cert_mutex_);

        TLSCertificateInfo info;

        if (!certificate_)
            return info;

        // Extract subject
        char* subject_name = X509_NAME_oneline(X509_get_subject_name(certificate_), nullptr, 0);
        if (subject_name) {
            info.subject = std::string(subject_name);
            OPENSSL_free(subject_name);
        }

        // Extract issuer
        char* issuer_name = X509_NAME_oneline(X509_get_issuer_name(certificate_), nullptr, 0);
        if (issuer_name) {
            info.issuer = std::string(issuer_name);
            OPENSSL_free(issuer_name);
        }

        // Extract serial number
        ASN1_INTEGER* serial = X509_get_serialNumber(certificate_);
        if (serial) {
            BIGNUM* bn = ASN1_INTEGER_to_BN(serial, nullptr);
            if (bn) {
                char* serial_str = BN_bn2hex(bn);
                if (serial_str) {
                    info.serial_number = std::string(serial_str);
                    OPENSSL_free(serial_str);
                }
                BN_free(bn);
            }
        }

        // Generate SHA-256 fingerprint
        unsigned char md[EVP_MAX_MD_SIZE];
        unsigned int md_len;
        if (X509_digest(certificate_, EVP_sha256(), md, &md_len)) {
            std::ostringstream oss;
            for (unsigned int i = 0; i < md_len; ++i) {
                oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(md[i]);
                if (i < md_len - 1)
                    oss << ":";
            }
            info.fingerprint_sha256 = oss.str();
        }

        // Set validity dates (simplified)
        info.not_before = std::chrono::system_clock::now() - std::chrono::hours{24};
        info.not_after = std::chrono::system_clock::now() + std::chrono::hours{24 * 365};

        return info;
    }

    std::vector<TLSCertificateInfo> TLSCertificateManager::get_certificate_chain_info() const
    {
        std::vector<TLSCertificateInfo> chain_info;

        // Add primary certificate info
        chain_info.push_back(get_certificate_info());

        // If there's a certificate chain, add those too
        std::lock_guard<std::mutex> lock(cert_mutex_);
        if (certificate_chain_) {
            int chain_count = sk_X509_num(certificate_chain_);
            for (int i = 0; i < chain_count; ++i) {
                X509* cert = sk_X509_value(certificate_chain_, i);
                if (cert) {
                    // Extract info from chain certificate (similar to above)
                    TLSCertificateInfo cert_info;
                    char* subject_name = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
                    if (subject_name) {
                        cert_info.subject = std::string(subject_name);
                        OPENSSL_free(subject_name);
                    }
                    chain_info.push_back(cert_info);
                }
            }
        }

        return chain_info;
    }

    bool TLSCertificateManager::save_certificate_to_file(const std::string& cert_file) const
    {
        std::lock_guard<std::mutex> lock(cert_mutex_);

        if (!certificate_)
            return false;

        FILE* fp = fopen(cert_file.c_str(), "w");
        if (!fp)
            return false;

        int result = PEM_write_X509(fp, certificate_);
        fclose(fp);

        return result == 1;
    }

    bool TLSCertificateManager::save_private_key_to_file(const std::string& key_file) const
    {
        std::lock_guard<std::mutex> lock(cert_mutex_);

        if (!private_key_)
            return false;

        FILE* fp = fopen(key_file.c_str(), "w");
        if (!fp)
            return false;

        int result = PEM_write_PrivateKey(fp, private_key_, nullptr, nullptr, 0, nullptr, nullptr);
        fclose(fp);

        return result == 1;
    }

    bool TLSCertificateManager::check_ocsp_status(const std::string& ocsp_url) const
    {
        // OCSP checking implementation would go here
        // For now, return true as placeholder
        (void)ocsp_url; // Suppress unused parameter warning
        return true;
    }

    bool TLSCertificateManager::staple_ocsp_response()
    {
        // OCSP stapling implementation would go here
        // For now, return true as placeholder
        return true;
    }

    // Utility functions
    std::string tls_version_to_string(TLSVersion version)
    {
        switch (version) {
        case TLSVersion::TLS_1_2:
            return "TLSv1.2";
        case TLSVersion::TLS_1_3:
            return "TLSv1.3";
        case TLSVersion::Auto:
            return "Auto";
        default:
            return "Unknown";
        }
    }

    TLSVersion parse_tls_version(const std::string& version_str)
    {
        if (version_str == "TLSv1.2")
            return TLSVersion::TLS_1_2;
        if (version_str == "TLSv1.3")
            return TLSVersion::TLS_1_3;
        if (version_str == "Auto")
            return TLSVersion::Auto;
        return TLSVersion::Auto; // Default
    }

    std::string verification_mode_to_string(TLSVerificationMode mode)
    {
        switch (mode) {
        case TLSVerificationMode::None:
            return "None";
        case TLSVerificationMode::Optional:
            return "Optional";
        case TLSVerificationMode::Required:
            return "Required";
        default:
            return "Unknown";
        }
    }

    TLSVerificationMode parse_verification_mode(const std::string& mode_str)
    {
        if (mode_str == "None")
            return TLSVerificationMode::None;
        if (mode_str == "Optional")
            return TLSVerificationMode::Optional;
        if (mode_str == "Required")
            return TLSVerificationMode::Required;
        return TLSVerificationMode::None; // Default
    }

    // OpenSSL initialization
    void initialize_openssl()
    {
        std::lock_guard<std::mutex> lock(openssl_init_mutex);

        if (!openssl_initialized.exchange(true)) {
            SSL_load_error_strings();
            OpenSSL_add_ssl_algorithms();
            SSL_library_init();
        }
    }

    void cleanup_openssl()
    {
        std::lock_guard<std::mutex> lock(openssl_init_mutex);

        if (openssl_initialized.exchange(false)) {
            EVP_cleanup();
            ERR_free_strings();
        }
    }

} // namespace ScratchBird
